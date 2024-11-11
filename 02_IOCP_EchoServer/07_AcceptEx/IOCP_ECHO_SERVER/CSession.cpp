#include "pch.h"
#include "ServerConfig.h"
#include "CSession.h"
#include "CLanServer.h"

bool CSession::AcceptExCompleted()
{
    int retVal;
    int errVal;

	retVal = setsockopt(m_sSessionSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
		(char *)&g_Server->m_sListenSocket, sizeof(SOCKET));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"setsockopt(SO_UPDATE_ACCEPT_CONTEXT) ���� : %d", errVal);
		return FALSE;
	}

    CreateIoCompletionPort((HANDLE)m_sSessionSocket, g_Server->m_hIOCPHandle, (ULONG_PTR)this, 0);

    SOCKADDR_IN *clientAddr = (SOCKADDR_IN *)m_AcceptBuffer;
	WCHAR clientAddrBuf[16] = { 0, };
	InetNtop(AF_INET, &clientAddr->sin_addr, clientAddrBuf, 16);

    if (!g_Server->OnConnectionRequest(clientAddrBuf, ntohs(clientAddr->sin_port)))
        return FALSE;

	AcquireSRWLockExclusive(&g_Server->m_disconnectStackLock);
	USHORT index = g_Server->m_arrDisconnectIndex[g_Server->m_disconnectArrTop--];
	ReleaseSRWLockExclusive(&g_Server->m_disconnectStackLock);

    UINT64 combineId = CLanServer::CombineIndex(index, ++g_Server->m_iCurrentID);
    
    // ������ �̹� �Ҵ�Ǿ ��ȯ��
    Init(combineId);

    InterlockedIncrement(&g_Server->m_iSessionCount);
    InterlockedIncrement(&g_monitor.m_lAcceptTPS);
    g_Server->m_arrPSessions[index] = this;
}

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
    m_SendBuffer.Enqueue((char *)&message, sizeof(VOID *));
    InterlockedIncrement(&g_monitor.m_lSendTPS);
    // PostSend();
    return TRUE;
}

void CSession::SendCompleted(int size)
{
    // m_SendBuffer.MoveFront(size);

    // m_iSendCount�� �ϰ� �Ҵ� ������ ����
    // * ����ŷ I/O�� ���� Send�� ��û�� �����ͺ��� �� ������ ��Ȳ�� �߻� ����
    // * �񵿱� I/O�� ������ ���� ������ �Ϸ� ������ ������
    for (int count = 0; count < m_iSendCount; count++)
    {
		int offset = count * sizeof(VOID *);
		CSerializableBuffer *buffer;
		m_SendBuffer.Peek((char *)&buffer, sizeof(VOID *), offset);
        // ���� �� ����
        CSerializableBuffer::Free(buffer);
    }

    m_SendBuffer.MoveFront(m_iSendCount * sizeof(VOID *));

#ifdef POSTSEND_LOST_DEBUG
	UINT64 index = InterlockedIncrement(&sendIndex);
	sendDebug[index % 65535] = { index, (USHORT)GetCurrentThreadId(), 0xff, TRUE, 0 };
#endif
    InterlockedExchange(&m_iSendFlag, FALSE);
}

bool CSession::PostAcceptEx(int index)
{
    int retVal;
    int errVal;
    m_sSessionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_sSessionSocket == INVALID_SOCKET)
    {
        errVal = WSAGetLastError();
        g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"PostAcceptEx socket() ���� : %d", errVal);
        return FALSE;
    }

    InterlockedIncrement(&m_iIOCount);
    ZeroMemory(&m_AcceptExOverlapped, sizeof(OVERLAPPED));
    m_AcceptExOverlapped.m_Index = index;
    retVal = g_Server->m_lpfnAcceptEx(g_Server->m_sListenSocket, m_sSessionSocket
                        , m_AcceptBuffer, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, NULL, (LPWSAOVERLAPPED)&m_AcceptExOverlapped);
    if (retVal == FALSE)
    {
		errVal = WSAGetLastError();
		if (errVal != WSA_IO_PENDING)
		{
			if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET)
				g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"AcceptEx() Error : %d", errVal);

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

	int sendBufferUseSize = m_SendBuffer.GetUseSize();
    // ���� �������� ũ�⺸�� ������ send�� �������� ����
	if (sendBufferUseSize < sizeof(VOID *))
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

    // �ִ� ����� �� �ִ� ����
	WSABUF wsaBuf[WSASEND_MAX_BUFFER_COUNT];
	int useSize = m_SendBuffer.GetUseSize();

    if (useSize < sizeof(VOID *))
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

    // �ݺ��� ���� WSABUF�� ��� �۾� ����
    m_iSendCount = min(useSize / sizeof(VOID *), WSASEND_MAX_BUFFER_COUNT);
    
    int count = 0;
    for (count = 0; count < m_iSendCount; count++)
    {
        // ���� ������ �ִ� ��Ȳ

        int offset = count * sizeof(VOID *);
        CSerializableBuffer *buffer;
        m_SendBuffer.Peek((char *)&buffer, sizeof(VOID *), offset);
        wsaBuf[count].buf = buffer->GetBufferPtr();
        wsaBuf[count].len = buffer->GetFullSize();
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
