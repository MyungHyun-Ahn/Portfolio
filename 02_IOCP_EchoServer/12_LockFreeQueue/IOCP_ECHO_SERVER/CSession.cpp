#include "pch.h"
#include "CSession.h"
#include "CLanServer.h"

void CSession::RecvCompleted(int size)
{
    m_RecvBuffer.MoveRear(size);
    InterlockedIncrement(&g_monitor.m_lRecvTPS);
    DWORD currentUseSize = m_RecvBuffer.GetUseSize();

    CSerializableBuffer *buffer = CSerializableBuffer::Alloc();

    while (currentUseSize > 0)
    {
        USHORT packetHeader;
        m_RecvBuffer.Peek((char *)&packetHeader, PACKET_HEADER_SIZE);

        if (m_RecvBuffer.GetUseSize() < PACKET_HEADER_SIZE + packetHeader)
            break;
        m_RecvBuffer.MoveFront(PACKET_HEADER_SIZE);

        if (packetHeader != sizeof(__int64))
            __debugbreak();

        {
            buffer->Clear();
            int retVal = m_RecvBuffer.Dequeue(buffer->GetContentBufferPtr(), packetHeader);
			if (retVal != sizeof(__int64))
				__debugbreak();

            buffer->MoveWritePos(retVal);

            g_Server->OnRecv(m_uiSessionID, buffer);
        }

        currentUseSize = m_RecvBuffer.GetUseSize();
    }

    CSerializableBuffer::Free(buffer);
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

    WSABUF wsaBuf[2];
    wsaBuf[0].buf = m_RecvBuffer.GetFrontPtr();
    wsaBuf[0].len = m_RecvBuffer.DirectEnqueueSize();
    wsaBuf[1].buf = m_RecvBuffer.GetPQueuePtr();
    wsaBuf[1].len = m_RecvBuffer.GetFreeSize() - wsaBuf[0].len;

    ZeroMemory(&m_RecvOverlapped, sizeof(OVERLAPPED));

    InterlockedIncrement(&m_iIOCount);

    DWORD flag = 0;
    retVal = WSARecv(m_sSessionSocket, wsaBuf, 2, nullptr, &flag, (LPWSAOVERLAPPED)&m_RecvOverlapped, NULL);
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

    // Sleep(0);

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
        // Sleep(0);
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
