#include "pch.h"
#include "CSession.h"
#include "CLanServer.h"

void CSession::RecvCompleted(int size)
{
    m_pRecvBuffer->MoveRear(size);
    InterlockedIncrement(&g_monitor.m_lRecvTPS);

    std::vector<CSerializableBufferView *> viewList;

    bool delayFlag = false;

	// 이전에서 딜레이 된게 있는 경우
	if (m_pDelayedBuffer != nullptr)
	{
		// 딜레이 된걸 처리해야함
		USHORT delayedHeaderSize = m_pDelayedBuffer->isReadHeaderSize();
		// 헤더 사이즈 부족
		if (delayedHeaderSize < (int)DEFINE::HEADER_SIZE)
		{
			// 헤더가 딜레이 됐나?
			// 딜레이 됐다면 여기에 모자른 만큼써줌
			int requireHeaderSize = (int)DEFINE::HEADER_SIZE - delayedHeaderSize;
			// 헤더가 2번씩 밀릴일은 없을 것임
			// 짤린 헤더를 써주고
			m_pDelayedBuffer->WriteDelayedHeader(m_pRecvBuffer->GetFrontPtr(), requireHeaderSize);
			m_pRecvBuffer->MoveFront(requireHeaderSize);
			// 네트워크 헤더 파악
			// - 지금은 USHORT 나중에 헤더로 정의하면 여기를 변경
			USHORT header;
			m_pDelayedBuffer->GetDelayedHeader((char *)&header, sizeof(USHORT));

			// 여기서 버퍼를 할당
			// - 헤더 사이즈 포함
			m_pDelayedBuffer->InitAndAlloc(header + sizeof(USHORT));
			m_pDelayedBuffer->Copy((char *)&header, 0, sizeof(USHORT));
		}
        else
        {
            if (m_pDelayedBuffer->isInit == 0)
                __debugbreak();
        }

		USHORT header;
		m_pDelayedBuffer->GetHeader((char *)&header, sizeof(USHORT));

		int requireSize = header - m_pDelayedBuffer->GetDataSize();
		// 데이터 모자름
		int recvBufferUseSize = m_pRecvBuffer->GetUseSize();
		if (recvBufferUseSize < requireSize)
		{
			delayFlag = true;
			m_pDelayedBuffer->Copy(m_pRecvBuffer->GetPQueuePtr(), recvBufferUseSize);
			m_pRecvBuffer->MoveFront(recvBufferUseSize);
		}
        else
        {
            // 필요한만큼 쓸 수 있으면
            m_pDelayedBuffer->Copy(m_pRecvBuffer->GetFrontPtr(), requireSize);
            m_pRecvBuffer->MoveFront(requireSize);
            viewList.push_back(m_pDelayedBuffer);
            m_pDelayedBuffer = nullptr;
        }
	}

    DWORD currentUseSize = m_pRecvBuffer->GetUseSize();
    while (currentUseSize > 0)
    {
        USHORT packetHeader;
        CSerializableBufferView *view = CSerializableBufferView::Alloc();

        if (currentUseSize < PACKET_HEADER_SIZE)
        {
            // Peek 실패 delayedBuffer로
            delayFlag = true;

            int remainSize = m_pRecvBuffer->GetUseSize();
            // 남은 사이즈 만큼 씀
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
            // 직접 할당해서 뒤로 밀기 Delayed
            delayFlag = true;
            // 헤더는 읽은 것임
            // front 부터 끝까지 복사
            view->InitAndAlloc(packetHeader + sizeof(USHORT));
            view->Copy(m_pRecvBuffer->GetFrontPtr(), 0, useSize);
            m_pRecvBuffer->MoveFront(useSize);
            m_pDelayedBuffer = view;

            // g_Logger->WriteLog(L"RecvBuffer", LOG_LEVEL::DEBUG, L"PacketDelay");
            break;
        }

        // 실질적으로 Recv 버퍼 입장에서는 읽은 것
        // - 하나의 메시지가 완전히 완성된 상태
        int offsetStart = m_pRecvBuffer->GetFrontOffset();
        int offsetEnd = offsetStart + packetHeader + PACKET_HEADER_SIZE;

        // 이 view를 그냥 써도 됨
        // Init 에서 RecvBuffer의 Ref가 증가
        view->Init(m_pRecvBuffer, offsetStart, offsetEnd);

        m_pRecvBuffer->MoveFront(PACKET_HEADER_SIZE + packetHeader);

        viewList.push_back(view);

        currentUseSize = m_pRecvBuffer->GetUseSize();
    }

    if (!delayFlag)
    {
        m_pDelayedBuffer = nullptr;
    }

    for (int i = 0; i < viewList.size(); i++)
    {
        g_Server->OnRecv(m_uiSessionID, viewList[i]);
    }

    // Ref 감소
    if (m_pRecvBuffer->DecreaseRef() == 0)
        CRecvBuffer::Free(m_pRecvBuffer);
}

// 인큐할 때 직렬화 버퍼의 포인터를 인큐
// * 나중에 Send 링버퍼는 락프리 큐로 교체될 것
bool CSession::SendPacket(CSerializableBuffer *message)
{
    m_lfQueue.Enqueue(message);
    return TRUE;
}

void CSession::SendCompleted(int size)
{
    // m_SendBuffer.MoveFront(size);

    // m_iSendCount를 믿고 할당 해제를 진행
    // * 논블락킹 I/O일 때만 Send를 요청한 데이터보다 덜 보내는 상황이 발생 가능
    // * 비동기 I/O는 무조건 전부 보내고 완료 통지가 도착함
    int count;
    for (count = 0; count < m_iSendCount; count++)
    {
        // 보낸 거 삭제
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

			// 사실 여기선 0이 될 일이 없음
			// 반환값을 사용안해도 됨
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

    // 여기서 얻은 만큼 쓸 것
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
        // 못꺼낸 것
        if (!m_lfQueue.Dequeue(&pBuffer))
        {
            g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"LFQueue::Dequeue() Error");
            // 말도 안되는 상황
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

            // 사실 여기선 0이 될 일이 없음
            // 반환값을 사용안해도 됨
			if (InterlockedDecrement(&m_iIOCount) == 0)
			{
				return FALSE;
			}
		}
    }

    return TRUE;
}
