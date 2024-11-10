#include "pch.h"
#include "ServerConfig.h"
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

bool CSession::SendPacket(CSerializableBuffer *message)
{
    m_SendBuffer.Enqueue(message->GetBufferPtr(), message->GetFullSize());
    InterlockedIncrement(&g_monitor.m_lSendTPS);
    // PostSend();
    return TRUE;
}

void CSession::SendCompleted(int size)
{
    m_SendBuffer.MoveFront(size);

#ifdef POSTSEND_LOST_DEBUG
	UINT64 index = InterlockedIncrement(&sendIndex);
	sendDebug[index % 65535] = { index, (USHORT)GetCurrentThreadId(), 0xff, TRUE, 0 };
#endif
    InterlockedExchange(&m_iSendFlag, FALSE);
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

bool CSession::PostSend(USHORT wher)
{
    int errVal;
    int retVal;

    // ���⼭ ��� 0�� ���� �ʾƵ� ���ʿ��� 0�� ���� ��Ȳ�� �߻�
	int sendBufferUseSize = m_SendBuffer.GetUseSize();
	if (sendBufferUseSize <= 0)
	{
		// if (sendBufferUseSize != m_SendBuffer.GetUseSize())
		//     __debugbreak();
		return TRUE;
	}

    if (InterlockedExchange(&m_iSendFlag, TRUE) == TRUE)
    {
#ifdef POSTSEND_LOST_DEBUG
        UINT64 index = InterlockedIncrement(&sendIndex);
        sendDebug[index % 65535] = { index, (USHORT)GetCurrentThreadId(), wher, FALSE, 2 };
#endif
        return TRUE;
    }

	WSABUF wsaBuf[2];
	int wsaBufCount = 1;
	int useSize = m_SendBuffer.GetUseSize();

    if (useSize <= 0)
    {
#ifdef POSTSEND_LOST_DEBUG
		UINT64 index = InterlockedIncrement(&sendIndex);
		sendDebug[index % 65535] = { index, (USHORT)GetCurrentThreadId(), wher, FALSE, 1 };
#endif
        InterlockedExchange(&m_iSendFlag, FALSE);
        return TRUE;
    }

#ifdef POSTSEND_LOST_DEBUG
    // ������� ������ ����
	UINT64 index = InterlockedIncrement(&sendIndex);
	sendDebug[index % 65535] = { index, (USHORT)GetCurrentThreadId(), wher, TRUE, 0 };
#endif
    // ���⼭ �̸� ���� ������ ����
    // - SendPacket���� sendBuffer�� Enqueue �Ǿ ���� ����
	int directDequeueSize = m_SendBuffer.DirectDequeueSize();

    if (useSize <= directDequeueSize)
    {
		wsaBuf[0].buf = m_SendBuffer.GetFrontPtr();
		wsaBuf[0].len = useSize;
		wsaBufCount = 1;
    }
	else
	{
		wsaBuf[0].buf = m_SendBuffer.GetFrontPtr();
		wsaBuf[0].len = directDequeueSize;
		wsaBuf[1].buf = m_SendBuffer.GetPQueuePtr();
		wsaBuf[1].len = useSize - wsaBuf[0].len;
		wsaBufCount = 2;
	}

    ZeroMemory(&m_SendOverlapped, sizeof(OVERLAPPED));

    InterlockedIncrement(&m_iIOCount);

    retVal = WSASend(m_sSessionSocket, wsaBuf, wsaBufCount, nullptr, 0, (LPOVERLAPPED)&m_SendOverlapped, NULL);
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
