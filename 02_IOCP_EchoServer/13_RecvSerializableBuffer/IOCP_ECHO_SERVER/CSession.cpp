#include "pch.h"
#include "CSession.h"
#include "CLanServer.h"

void CSession::RecvCompleted(int size)
{
    m_pRecvBuffer->MoveRear(size);
    InterlockedIncrement(&g_monitor.m_lRecvTPS);
    DWORD currentUseSize = m_pRecvBuffer->GetUseSize();

    std::vector<CSerializableBufferView *> viewList;

    bool delayFlag = false;

    while (currentUseSize > 0)
    {
        USHORT packetHeader;
        CSerializableBufferView *view = CSerializableBufferView::Alloc();

        // �������� ������ �Ȱ� �ִ� ���
        if (m_pDelayedBuffer != nullptr)
        {
            // ������ �Ȱ� ó���ؾ���
            USHORT delayedHeaderSize = m_pDelayedBuffer->isReadHeaderSize();
            // ��� ������ ����
            if (delayedHeaderSize < (int)DEFINE::HEADER_SIZE)
            {
                // ����� ������ �Ƴ�?
                // ������ �ƴٸ� ���⿡ ���ڸ� ��ŭ����
                int requireHeaderSize = (int)DEFINE::HEADER_SIZE - delayedHeaderSize;
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
                // ��� ����
                continue;
            }
            else
            {
                USHORT header;
                m_pDelayedBuffer->GetHeader((char *)&header, sizeof(header));

                int requireSize = header - m_pDelayedBuffer->GetDataSize();
                // ������ ���ڸ�
                int recvBufferUseSize = m_pRecvBuffer->GetUseSize();
                if (recvBufferUseSize < requireSize)
                {
                    m_pDelayedBuffer->Copy(m_pRecvBuffer->GetPQueuePtr(), recvBufferUseSize);
                    CSerializableBufferView::Free(view);
                    break;
                }
                
                // �ʿ��Ѹ�ŭ �� �� ������
                m_pDelayedBuffer->Copy(m_pRecvBuffer->GetPQueuePtr(), requireSize);
                m_pRecvBuffer->MoveFront(requireSize);

                viewList.push_back(m_pDelayedBuffer);
                m_pDelayedBuffer = nullptr;
                continue;
            }
        }


        if (m_pRecvBuffer->Peek((char *)&packetHeader, PACKET_HEADER_SIZE) == -1)
        {
            // Peek ���� delayedBuffer��
            delayFlag = true;

            int remainSize = m_pRecvBuffer->GetUseSize();
            // ���� ������ ��ŭ ��
            view->WriteDelayedHeader(m_pRecvBuffer->GetFrontPtr(), remainSize);

            m_pDelayedBuffer = view;
            break;
        }

        if (m_pRecvBuffer->GetUseSize() < PACKET_HEADER_SIZE + packetHeader)
        {
            // ���� �Ҵ��ؼ� �ڷ� �б� Delayed
            delayFlag = true;
            // ����� ���� ����
            // front ���� ������ ����
            view->InitAndAlloc(packetHeader + sizeof(USHORT));
            view->Copy(m_pRecvBuffer->GetFrontPtr(), 0, m_pRecvBuffer->GetUseSize());
            m_pDelayedBuffer = view;
            break;
        }

        // ���������� Recv ���� ���忡���� ���� ��
        // - �ϳ��� �޽����� ������ �ϼ��� ����
        int offsetStart = m_pRecvBuffer->GetFrontOffset();
        int offsetEnd = offsetStart + packetHeader + PACKET_HEADER_SIZE;

        // �� view�� �׳� �ᵵ ��
        // Init ���� RecvBuffer�� Ref�� ����
        view->Init(m_pRecvBuffer, offsetStart, offsetEnd);
        viewList.push_back(view);


        m_pRecvBuffer->MoveFront(PACKET_HEADER_SIZE + packetHeader);
        currentUseSize = m_pRecvBuffer->GetUseSize();
    }

    if (!delayFlag)
    {
		for (int i = 0; i < viewList.size(); i++)
		{
			g_Server->OnRecv(m_uiSessionID, viewList[i]);
		}

        m_pDelayedBuffer = nullptr;
    }
    // Ref ����
    if (m_pRecvBuffer->DecreaseRef() == 0)
        CRecvBuffer::Free(m_pRecvBuffer);
}

// ��ť�� �� ����ȭ ������ �����͸� ��ť
// * ���߿� Send �����۴� ������ ť�� ��ü�� ��
bool CSession::SendPacket(CSerializableBuffer *message)
{
    m_lfQueue.Enqueue(message);
    return TRUE;
}

void CSession::SendCompleted(int size)
{
    // m_SendBuffer.MoveFront(size);

    // m_iSendCount�� �ϰ� �Ҵ� ������ ����
    // * ����ŷ I/O�� ���� Send�� ��û�� �����ͺ��� �� ������ ��Ȳ�� �߻� ����
    // * �񵿱� I/O�� ������ ���� ������ �Ϸ� ������ ������
    int count;
    for (count = 0; count < m_iSendCount; count++)
    {
        // ���� �� ����
        CSerializableBuffer::Free(m_arrPSendBufs[count]);
    }

	if (count != m_iSendCount)
	{
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"CSession::SendCompleted %d != %d", count + 1, m_iSendCount);
	}

    m_iSendCount = 0;

#ifdef POSTSEND_LOST_DEBUG
	UINT64 index = InterlockedIncrement(&sendIndex);
	sendDebug[index % 65535] = { index, (USHORT)GetCurrentThreadId(), 0xff, TRUE, 0 };
#endif
    InterlockedIncrement(&g_monitor.m_lSendTPS);

    if (!PostSend(TRUE))
    {
		InterlockedExchange(&m_iSendFlag, FALSE);
    }
}

bool CSession::PostRecv()
{
    int errVal;
    int retVal;

    m_pRecvBuffer = CRecvBuffer::Alloc();
    m_pRecvBuffer->IncreaseRef();
    WSABUF wsaBuf;
    wsaBuf.buf = m_pRecvBuffer->GetPQueuePtr();
    wsaBuf.len = m_pRecvBuffer->GetCapacity();

    ZeroMemory(&m_RecvOverlapped, sizeof(OVERLAPPED));

    InterlockedIncrement(&m_iIOCount);

    DWORD flag = 0;
    retVal = WSARecv(m_sSessionSocket, &wsaBuf, 1, nullptr, &flag, (LPWSAOVERLAPPED)&m_RecvOverlapped, NULL);
    if (retVal == SOCKET_ERROR)
    {
        errVal = WSAGetLastError();
        if (errVal != WSA_IO_PENDING)
        {
            if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET)
                g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"WSARecv() Error : %d", errVal);

			// ��� ���⼱ 0�� �� ���� ����
			// ��ȯ���� �����ص� ��
            if (InterlockedDecrement(&m_iIOCount) == 0)
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

bool CSession::PostSend(BOOL isCompleted)
{
    int errVal;
    int retVal;

	int sendUseSize = m_lfQueue.GetUseSize();
	if (sendUseSize <= 0)
	{
		return FALSE;
	}

    if (!isCompleted)
    {
		if (InterlockedExchange(&m_iSendFlag, TRUE) == TRUE)
		{
#ifdef POSTSEND_LOST_DEBUG
			UINT64 index = InterlockedIncrement(&sendIndex);
			sendDebug[index % 65535] = { index, (USHORT)GetCurrentThreadId(), wher, FALSE, 2 };
#endif
			return TRUE;
		}
    }

    // ���⼭ ���� ��ŭ �� ��
	sendUseSize = m_lfQueue.GetUseSize();
	if (sendUseSize <= 0)
	{
        InterlockedExchange(&m_iSendFlag, FALSE);
		return FALSE;
	}
	
    WSABUF wsaBuf[WSASEND_MAX_BUFFER_COUNT];

    m_iSendCount = min(sendUseSize, WSASEND_MAX_BUFFER_COUNT);

    int count;
    for (count = 0; count < m_iSendCount; count++)
    {
        CSerializableBuffer *pBuffer;
        // ������ ��
        if (!m_lfQueue.Dequeue(&pBuffer))
        {
            g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"LFQueue::Dequeue() Error");
            // ���� �ȵǴ� ��Ȳ
            CCrashDump::Crash();
        }
        
        wsaBuf[count].buf = pBuffer->GetBufferPtr();
        wsaBuf[count].len = pBuffer->GetFullSize();

        m_arrPSendBufs[count] = pBuffer;
    }

    if (count != m_iSendCount)
    {
        g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"CSession::PostSend %d != %d", count + 1, m_iSendCount);
    }

    ZeroMemory(&m_SendOverlapped, sizeof(OVERLAPPED));
    
    InterlockedIncrement(&m_iIOCount);

    retVal = WSASend(m_sSessionSocket, wsaBuf, m_iSendCount, nullptr, 0, (LPOVERLAPPED)&m_SendOverlapped, NULL);
    if (retVal == SOCKET_ERROR)
    {
        errVal = WSAGetLastError();
		if (errVal != WSA_IO_PENDING)
		{
            if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET)
			    g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"WSASend() Error : %d", errVal);

            // ��� ���⼱ 0�� �� ���� ����
            // ��ȯ���� �����ص� ��
			if (InterlockedDecrement(&m_iIOCount) == 0)
			{
				return FALSE;
			}
		}
    }

    return TRUE;
}
