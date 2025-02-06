#include "pch.h"
#include "ServerSetting.h"
#include "CLanSession.h"
#include "CLanServer.h"

void CLanSession::RecvCompleted(int size) noexcept
{
    m_pRecvBuffer->MoveRear(size);

    bool delayFlag = false;

	// �������� ������ �Ȱ� �ִ� ���
	if (m_pDelayedBuffer != nullptr)
	{
		// ������ �Ȱ� ó���ؾ���
		USHORT delayedHeaderSize = m_pDelayedBuffer->isReadHeaderSize();
		// ��� ������ ����
		if (delayedHeaderSize < (int)CSerializableBuffer<TRUE>::DEFINE::HEADER_SIZE)
		{
			// ����� ������ �Ƴ�?
			// ������ �ƴٸ� ���⿡ ���ڸ� ��ŭ����
			int requireHeaderSize = (int)CSerializableBuffer<TRUE>::DEFINE::HEADER_SIZE - delayedHeaderSize;
			// ����� 2���� �и����� ���� ����
			// ©�� ����� ���ְ�
			m_pDelayedBuffer->WriteDelayedHeader(m_pRecvBuffer->GetFrontPtr(), requireHeaderSize);
			m_pRecvBuffer->MoveFront(requireHeaderSize);
			// ��Ʈ��ũ ��� �ľ�
			// - ������ USHORT ���߿� ����� �����ϸ� ���⸦ ����
			USHORT header;
			m_pDelayedBuffer->GetDelayedHeader((char *)&header, sizeof(USHORT));

			// ���⼭ ���۸� �Ҵ�
			// - ��� ������ ����
			m_pDelayedBuffer->InitAndAlloc(header + sizeof(USHORT));
			m_pDelayedBuffer->Copy((char *)&header, 0, sizeof(USHORT));
		}

		USHORT header;
		m_pDelayedBuffer->GetHeader((char *)&header, sizeof(USHORT));

		int requireSize = header - m_pDelayedBuffer->GetDataSize();
		// ������ ���ڸ�
		int recvBufferUseSize = m_pRecvBuffer->GetUseSize();
		if (recvBufferUseSize < requireSize)
		{
			delayFlag = true;
			m_pDelayedBuffer->Copy(m_pRecvBuffer->GetPQueuePtr(), recvBufferUseSize);
			m_pRecvBuffer->MoveFront(recvBufferUseSize);
		}
        else
        {
            // �ʿ��Ѹ�ŭ �� �� ������
            m_pDelayedBuffer->Copy(m_pRecvBuffer->GetFrontPtr(), requireSize);
            m_pRecvBuffer->MoveFront(requireSize);
            InterlockedIncrement(&g_monitor.m_lRecvTPS);
            m_pDelayedBuffer->m_uiSessionId = m_uiSessionID;
            g_LanServer->OnRecv(m_uiSessionID, CSmartPtr<CSerializableBufferView<TRUE>>(m_pDelayedBuffer));
            m_pDelayedBuffer = nullptr;
        }
	}

    DWORD currentUseSize = m_pRecvBuffer->GetUseSize();
    while (currentUseSize > 0)
    {
        USHORT packetHeader;
        CSerializableBufferView<TRUE> *view = CSerializableBufferView<TRUE>::Alloc();
        
        if (currentUseSize < (int)CSerializableBuffer<TRUE>::DEFINE::HEADER_SIZE)
        {
            // Peek ���� delayedBuffer��
            delayFlag = true;

            int remainSize = m_pRecvBuffer->GetUseSize();
            // ���� ������ ��ŭ ��
            view->WriteDelayedHeader(m_pRecvBuffer->GetFrontPtr(), remainSize);
            m_pRecvBuffer->MoveFront(remainSize);
            m_pDelayedBuffer = view;

            // g_Logger->WriteLog(L"RecvBuffer", LOG_LEVEL::DEBUG, L"HeaderDelay");
            break;
        }

        int useSize = m_pRecvBuffer->GetUseSize();
        m_pRecvBuffer->Peek((char *)&packetHeader, PACKET_HEADER_SIZE);
        if (useSize < PACKET_HEADER_SIZE + packetHeader)
        {
            // ���� �Ҵ��ؼ� �ڷ� �б� Delayed
            delayFlag = true;
            // ����� ���� ����
            // front ���� ������ ����
            view->InitAndAlloc(packetHeader + sizeof(USHORT));
            view->Copy(m_pRecvBuffer->GetFrontPtr(), 0, useSize);
            m_pRecvBuffer->MoveFront(useSize);
            m_pDelayedBuffer = view;

            // g_Logger->WriteLog(L"RecvBuffer", LOG_LEVEL::DEBUG, L"PacketDelay");
            break;
        }

        // ���������� Recv ���� ���忡���� ���� ��
        // - �ϳ��� �޽����� ������ �ϼ��� ����
        int offsetStart = m_pRecvBuffer->GetFrontOffset();
        int offsetEnd = offsetStart + packetHeader + PACKET_HEADER_SIZE;

        // �� view�� �׳� �ᵵ ��
        // Init ���� RecvBuffer�� Ref�� ����
        view->Init(m_pRecvBuffer, offsetStart, offsetEnd);

        m_pRecvBuffer->MoveFront(PACKET_HEADER_SIZE + packetHeader);
        InterlockedIncrement(&g_monitor.m_lRecvTPS);
        view->m_uiSessionId = m_uiSessionID;
        g_LanServer->OnRecv(m_uiSessionID, view);

        currentUseSize = m_pRecvBuffer->GetUseSize();
    }

    if (!delayFlag)
    {
        m_pDelayedBuffer = nullptr;
    }

    // Ref ����
    if (m_pRecvBuffer->DecreaseRef() == 0)
        CRecvBuffer::Free(m_pRecvBuffer);
}

// ��ť�� �� ����ȭ ������ �����͸� ��ť
bool CLanSession::SendPacket(CSerializableBuffer<TRUE> *message) noexcept
{
    // ���⼭ �ö� RefCount�� SendCompleted���� ������
    // Ȥ�� ReleaseSession
    message->IncreaseRef();
    m_lfSendBufferQueue.Enqueue(message);
    return TRUE;
}

void CLanSession::SendCompleted(int size) noexcept
{
    InterlockedAdd(&g_monitor.m_lSendTPS, m_iSendCount);

    // m_iSendCount�� �ϰ� �Ҵ� ������ ����
    // * ����ŷ I/O�� ���� Send�� ��û�� �����ͺ��� �� ������ ��Ȳ�� �߻� ����
    // * �񵿱� I/O�� ������ ���� ������ �Ϸ� ������ ������
    int count;
    for (count = 0; count < m_iSendCount; count++)
    {
        // RefCount�� ���߰� 0�̶�� ���� �� ����
        if (m_arrPSendBufs[count]->DecreaseRef() == 0)
        {
            CSerializableBuffer<TRUE>::Free(m_arrPSendBufs[count]);
        }
    }

    m_iSendCount = 0;

#ifdef POSTSEND_LOST_DEBUG
	UINT64 index = InterlockedIncrement(&sendIndex);
	sendDebug[index % 65535] = { index, (USHORT)GetCurrentThreadId(), 0xff, TRUE, 0 };
#endif

    PostSend(TRUE);
}

bool CLanSession::PostRecv() noexcept
{
    int errVal;
    int retVal;

    // ���⼭�� ���� Ref ������ �ʿ�
    // * WSARecv���� Ref ������ �Ұ���

	InterlockedIncrement(&m_iIOCountAndRelease);
	if ((m_iIOCountAndRelease & CLanSession::RELEASE_FLAG) == CLanSession::RELEASE_FLAG)
	{
		InterlockedDecrement(&m_iIOCountAndRelease);
		return FALSE;
	}

    m_pRecvBuffer = CRecvBuffer::Alloc();
    m_pRecvBuffer->IncreaseRef();
    WSABUF wsaBuf;
    wsaBuf.buf = m_pRecvBuffer->GetPQueuePtr();
    wsaBuf.len = m_pRecvBuffer->GetCapacity();

    ZeroMemory((m_pMyOverlappedStartAddr + 1), sizeof(OVERLAPPED));

    DWORD flag = 0;
    {
        // PROFILE_BEGIN(0, "WSARecv");
		retVal = WSARecv(m_sSessionSocket, &wsaBuf, 1, nullptr, &flag, (LPWSAOVERLAPPED)(m_pMyOverlappedStartAddr + 1), NULL);
    }
    if (retVal == SOCKET_ERROR)
    {
        errVal = WSAGetLastError();
        if (errVal != WSA_IO_PENDING)
        {
            if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET && errVal != WSAEINTR)
                g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSARecv() Error : %d", errVal);

			// ��� ���⼱ 0�� �� ���� ����
			// ��ȯ���� �����ص� ��
            if (InterlockedDecrement(&m_iIOCountAndRelease) == 0)
            {
                return FALSE;
            }
        }
		else
		{
			if (m_iCacelIoCalled)
			{
				CancelIoEx((HANDLE)m_sSessionSocket, nullptr);
				return FALSE;
			}
		}
	}

    return TRUE;
}

bool CLanSession::PostSend(BOOL isCompleted) noexcept
{
    int errVal;
    int retVal;
    int sendUseSize;

    if (!isCompleted)
    {
		sendUseSize = m_lfSendBufferQueue.GetUseSize();
		if (sendUseSize <= 0)
		{
			return FALSE;
	}

		if (InterlockedExchange(&m_iSendFlag, TRUE) == TRUE)
		{
#ifdef POSTSEND_LOST_DEBUG
			UINT64 index = InterlockedIncrement(&sendIndex);
			sendDebug[index % 65535] = { index, (USHORT)GetCurrentThreadId(), wher, FALSE, 2 };
#endif
			return FALSE;
		}
    }

    // ���⼭ ���� ��ŭ �� ��
	sendUseSize = m_lfSendBufferQueue.GetUseSize();
	if (sendUseSize <= 0)
	{
        InterlockedExchange(&m_iSendFlag, FALSE);
		return FALSE;
	}
	
	InterlockedIncrement(&m_iIOCountAndRelease);
	if ((m_iIOCountAndRelease & CLanSession::RELEASE_FLAG) == CLanSession::RELEASE_FLAG)
	{
		InterlockedDecrement(&m_iIOCountAndRelease);
        InterlockedExchange(&m_iSendFlag, FALSE);
		return FALSE;
	}

    WSABUF wsaBuf[WSASEND_MAX_BUFFER_COUNT];

    m_iSendCount = min(sendUseSize, WSASEND_MAX_BUFFER_COUNT);

	// WSASEND_MAX_BUFFER_COUNT ��ŭ 1�ʿ� ��� �������� ī��Ʈ
	// �� ��ġ�� ���ٸ� �� �ø� ��
	if (m_iSendCount == WSASEND_MAX_BUFFER_COUNT)
		InterlockedIncrement(&g_monitor.m_lMaxSendCount);

    int count;
    for (count = 0; count < m_iSendCount; count++)
    {
        CSerializableBuffer<TRUE> *pBuffer;
        // ������ ��
        if (!m_lfSendBufferQueue.Dequeue(&pBuffer))
        {
            g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"LFQueue::Dequeue() Error");
            // ���� �ȵǴ� ��Ȳ
            CCrashDump::Crash();
        }
        
        wsaBuf[count].buf = pBuffer->GetBufferPtr();
        wsaBuf[count].len = pBuffer->GetFullSize();

        m_arrPSendBufs[count] = pBuffer;
    }

    ZeroMemory((m_pMyOverlappedStartAddr + 2), sizeof(OVERLAPPED));

	{
		// PROFILE_BEGIN(0, "WSASend");
		retVal = WSASend(m_sSessionSocket, wsaBuf, m_iSendCount, nullptr, 0, (LPOVERLAPPED)(m_pMyOverlappedStartAddr + 2), NULL);
	}
    if (retVal == SOCKET_ERROR)
    {
        errVal = WSAGetLastError();
		if (errVal != WSA_IO_PENDING)
		{
            if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET && errVal != WSAEINTR)
            {
                __debugbreak();
                g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSASend() Error : %d", errVal);
            }
            // ��� ���⼱ 0�� �� ���� ����
            // ��ȯ���� �����ص� ��
			if (InterlockedDecrement(&m_iIOCountAndRelease) == 0)
			{
				return FALSE;
			}
		}
		else
		{
			if (m_iCacelIoCalled)
			{
				CancelIoEx((HANDLE)m_sSessionSocket, nullptr);
				return FALSE;
			}
		}
    }

    return TRUE;
}

void CLanSession::Clear() noexcept
{
	// ReleaseSession ��ÿ� �����ִ� send �����۸� Ȯ��
	// * �����ִ� ��찡 Ȯ�ε�
	// * ���� ����ȭ ���۸� �Ҵ� �����ϰ� ���� ����


	for (int count = 0; count < m_iSendCount; count++)
	{
		if (m_arrPSendBufs[count]->DecreaseRef() == 0)
		{
			CSerializableBuffer<TRUE>::Free(m_arrPSendBufs[count]);
		}
	}

	LONG useBufferSize = m_lfSendBufferQueue.GetUseSize();
	for (int i = 0; i < useBufferSize; i++)
	{
		CSerializableBuffer<TRUE> *pBuffer;
		// ������ ��
		if (!m_lfSendBufferQueue.Dequeue(&pBuffer))
		{
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"LFQueue::Dequeue() Error");
			// ���� �ȵǴ� ��Ȳ
			CCrashDump::Crash();
		}

		// RefCount�� ���߰� 0�̶�� ���� �� ����
		if (pBuffer->DecreaseRef() == 0)
		{
			CSerializableBuffer<TRUE>::Free(pBuffer);
		}
	}

	useBufferSize = m_lfSendBufferQueue.GetUseSize();
	if (useBufferSize != 0)
	{
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"LFQueue is not empty Error");
	}

	if (m_pRecvBuffer != nullptr)
	{
		if (m_pRecvBuffer->DecreaseRef() == 0)
			CRecvBuffer::Free(m_pRecvBuffer);
	}

	m_sSessionSocket = INVALID_SOCKET;
	m_uiSessionID = 0;
	m_pRecvBuffer = nullptr;
}
