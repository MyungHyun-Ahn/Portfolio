#include "pch.h"
#include "ServerSetting.h"
#include "CLanSession.h"
#include "CLanServer.h"

void CLanSession::RecvCompleted(int size) noexcept
{
    m_pRecvBuffer->MoveRear(size);

    bool delayFlag = false;

	// 이전에서 딜레이 된게 있는 경우
	if (m_pDelayedBuffer != nullptr)
	{
		// 딜레이 된걸 처리해야함
		USHORT delayedHeaderSize = m_pDelayedBuffer->isReadHeaderSize();
		// 헤더 사이즈 부족
		if (delayedHeaderSize < (int)CSerializableBuffer<TRUE>::DEFINE::HEADER_SIZE)
		{
			// 헤더가 딜레이 됐나?
			// 딜레이 됐다면 여기에 모자른 만큼써줌
			int requireHeaderSize = (int)CSerializableBuffer<TRUE>::DEFINE::HEADER_SIZE - delayedHeaderSize;
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
        InterlockedIncrement(&g_monitor.m_lRecvTPS);
        view->m_uiSessionId = m_uiSessionID;
        g_LanServer->OnRecv(m_uiSessionID, view);

        currentUseSize = m_pRecvBuffer->GetUseSize();
    }

    if (!delayFlag)
    {
        m_pDelayedBuffer = nullptr;
    }

    // Ref 감소
    if (m_pRecvBuffer->DecreaseRef() == 0)
        CRecvBuffer::Free(m_pRecvBuffer);
}

// 인큐할 때 직렬화 버퍼의 포인터를 인큐
bool CLanSession::SendPacket(CSerializableBuffer<TRUE> *message) noexcept
{
    // 여기서 올라간 RefCount는 SendCompleted에서 내려감
    // 혹은 ReleaseSession
	InterlockedIncrement(&g_monitor.m_lSendTPS);
    message->IncreaseRef();
	m_SendRingBuffer.Enqueue(message->GetBufferPtr(), message->GetFullSize());
	if (message->DecreaseRef() == 0)
		CSerializableBuffer<TRUE>::Free(message);

    return TRUE;
}

void CLanSession::SendCompleted(int size) noexcept
{
    // m_iSendCount를 믿고 할당 해제를 진행
    // * 논블락킹 I/O일 때만 Send를 요청한 데이터보다 덜 보내는 상황이 발생 가능
    // * 비동기 I/O는 무조건 전부 보내고 완료 통지가 도착함

	m_SendRingBuffer.MoveFront(size);

    // int count;
    // for (count = 0; count < m_iSendCount; count++)
    // {
    //     // RefCount를 낮추고 0이라면 보낸 거 삭제
    //     if (m_arrPSendBufs[count]->DecreaseRef() == 0)
    //     {
    //         CSerializableBuffer<TRUE>::Free(m_arrPSendBufs[count]);
    //     }
    // }

    // m_iSendCount = 0;

	LONG idx = InterlockedIncrement(&sendDebugIndex);
	sendDebug[idx % 65535] = { GetCurrentThreadId(), WHERE_SEND::RELEASE_SENDCOMPLETED };
	InterlockedExchange(&m_iSendFlag, FALSE);

    PostSend(WHERE_SEND::ACQUIRE_SENDCOMPLETED);
}

bool CLanSession::PostRecv() noexcept
{
    int errVal;
    int retVal;

    // 여기서는 수동 Ref 버전이 필요
    // * WSARecv에서 Ref 관리가 불가능

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

			// 사실 여기선 0이 될 일이 없음
			// 반환값을 사용안해도 됨
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

bool CLanSession::PostSend(WHERE_SEND where) noexcept
{
    int errVal;
    int retVal;
    int sendUseSize;

	sendUseSize = m_SendRingBuffer.GetUseSize();
	if (sendUseSize <= 0)
	{
		return FALSE;
	}

	Sleep(0);

	if (InterlockedExchange(&m_iSendFlag, TRUE) == TRUE)
	{
		return FALSE;
	}
	{
		LONG idx = InterlockedIncrement(&sendDebugIndex);
		sendDebug[idx % 65535] = { GetCurrentThreadId(), where };
	}
	sendUseSize = m_SendRingBuffer.GetUseSize();
	if (sendUseSize <= 0)
	{
		Sleep(0);
		{
			LONG idx = InterlockedIncrement(&sendDebugIndex);
			sendDebug[idx % 65535] = { GetCurrentThreadId(), WHERE_SEND::RELEASE_POSTSEND };
		}
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


    // WSABUF wsaBuf[WSASEND_MAX_BUFFER_COUNT];

    // m_iSendCount = min(sendUseSize, WSASEND_MAX_BUFFER_COUNT);

	// WSASEND_MAX_BUFFER_COUNT 만큼 1초에 몇번 보내는지 카운트
	// 이 수치가 높다면 더 늘릴 것

	WSABUF wsaBuf[2];
	int wsaBufCount = 1;
	int useSize = sendUseSize;
    int directDequeueSize = m_SendRingBuffer.DirectDequeueSize();

	if (useSize <= directDequeueSize)
	{
		wsaBuf[0].buf = m_SendRingBuffer.GetFrontPtr();
		wsaBuf[0].len = useSize;
		wsaBufCount = 1;
	}
	else
	{
		wsaBuf[0].buf = m_SendRingBuffer.GetFrontPtr();
		wsaBuf[0].len = directDequeueSize;
		wsaBuf[1].buf = m_SendRingBuffer.GetPQueuePtr();
		wsaBuf[1].len = useSize - wsaBuf[0].len;
		wsaBufCount = 2;
	}

    ZeroMemory((m_pMyOverlappedStartAddr + 2), sizeof(OVERLAPPED));

	{
		// PROFILE_BEGIN(0, "WSASend");
		retVal = WSASend(m_sSessionSocket, wsaBuf, wsaBufCount, nullptr, 0, (LPOVERLAPPED)(m_pMyOverlappedStartAddr + 2), NULL);
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
            // 사실 여기선 0이 될 일이 없음
            // 반환값을 사용안해도 됨
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
	// ReleaseSession 당시에 남아있는 send 링버퍼를 확인
	// * 남아있는 경우가 확인됨
	// * 남은 직렬화 버퍼를 할당 해제하고 세션 삭제


	// for (int count = 0; count < m_iSendCount; count++)
	// {
	// 	if (m_arrPSendBufs[count]->DecreaseRef() == 0)
	// 	{
	// 		CSerializableBuffer<TRUE>::Free(m_arrPSendBufs[count]);
	// 	}
	// }

	// LONG useBufferSize = m_lfSendBufferQueue.GetUseSize();
	// for (int i = 0; i < useBufferSize; i++)
	// {
	// 	CSerializableBuffer<TRUE> *pBuffer;
	// 	// 못꺼낸 것
	// 	if (!m_lfSendBufferQueue.Dequeue(&pBuffer))
	// 	{
	// 		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"LFQueue::Dequeue() Error");
	// 		// 말도 안되는 상황
	// 		CCrashDump::Crash();
	// 	}
	// 
	// 	// RefCount를 낮추고 0이라면 보낸 거 삭제
	// 	if (pBuffer->DecreaseRef() == 0)
	// 	{
	// 		CSerializableBuffer<TRUE>::Free(pBuffer);
	// 	}
	// }

	// useBufferSize = m_lfSendBufferQueue.GetUseSize();
	// if (useBufferSize != 0)
	// {
	// 	g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"LFQueue is not empty Error");
	// }

	if (m_pRecvBuffer != nullptr)
	{
		if (m_pRecvBuffer->DecreaseRef() == 0)
			CRecvBuffer::Free(m_pRecvBuffer);
	}

	m_sSessionSocket = INVALID_SOCKET;
	m_uiSessionID = 0;
    m_SendRingBuffer.Clear();
	m_pRecvBuffer = nullptr;
}
