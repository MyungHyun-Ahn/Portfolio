#include "pch.h"
#include "ClientSetting.h"
#include "CNetClient.h"
#include "COverlappedAllocator.h"

namespace NETWORK_CLIENT
{
	CNetClientManager *g_netClientMgr;

	void CNetSession::Clear() noexcept
	{
		// ReleaseSession 당시에 남아있는 send 링버퍼를 확인
		// * 남아있는 경우가 확인됨
		// * 남은 직렬화 버퍼를 할당 해제하고 세션 삭제

		if (m_pDelayedBuffer != nullptr)
		{
			CSerializableBufferView<FALSE>::Free(m_pDelayedBuffer);
			m_pDelayedBuffer = nullptr;
		}

		for (int count = 0; count < m_iSendCount; count++)
		{
			if (m_arrPSendBufs[count]->DecreaseRef() == 0)
			{
				CSerializableBuffer<FALSE>::Free(m_arrPSendBufs[count]);
			}
		}

		m_iSendCount = 0;

		LONG useBufferSize = m_lfSendBufferQueue.GetUseSize();
		for (int i = 0; i < useBufferSize; i++)
		{
			CSerializableBuffer<FALSE> *pBuffer;
			// 못꺼낸 것
			if (!m_lfSendBufferQueue.Dequeue(&pBuffer))
			{
				g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"LFQueue::Dequeue() Error");
				// 말도 안되는 상황
				CCrashDump::Crash();
			}

			// RefCount를 낮추고 0이라면 보낸 거 삭제
			if (pBuffer->DecreaseRef() == 0)
			{
				CSerializableBuffer<FALSE>::Free(pBuffer);
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

	void CNetSession::RecvCompleted(int size) noexcept
	{
		m_pRecvBuffer->MoveRear(size);

		bool delayFlag = false;

		// 이전에서 딜레이 된게 있는 경우
		if (m_pDelayedBuffer != nullptr)
		{
			// 딜레이 된걸 처리해야함
			USHORT delayedHeaderSize = m_pDelayedBuffer->isReadHeaderSize();
			// 헤더 사이즈 부족
			if (delayedHeaderSize < (int)CSerializableBuffer<FALSE>::DEFINE::HEADER_SIZE)
			{
				// 헤더가 딜레이 됐나?
				// 딜레이 됐다면 여기에 모자른 만큼써줌
				int requireHeaderSize = (int)CSerializableBuffer<FALSE>::DEFINE::HEADER_SIZE - delayedHeaderSize;
				// 헤더가 2번씩 밀릴일은 없을 것임
				// 짤린 헤더를 써주고
				m_pDelayedBuffer->WriteDelayedHeader(m_pRecvBuffer->GetFrontPtr(), requireHeaderSize);
				m_pRecvBuffer->MoveFront(requireHeaderSize);
				// 네트워크 헤더 파악
				// - 지금은 USHORT 나중에 헤더로 정의하면 여기를 변경
				NetHeader header;
				m_pDelayedBuffer->GetDelayedHeader((char *)&header, sizeof(NetHeader));

				// 여기서 버퍼를 할당
				// - 헤더 사이즈 포함
				m_pDelayedBuffer->InitAndAlloc(header.len + sizeof(NetHeader));
				m_pDelayedBuffer->Copy((char *)&header, 0, sizeof(NetHeader));
			}

			// header는 다 읽힌 것
			NetHeader header;
			m_pDelayedBuffer->GetHeader((char *)&header, sizeof(NetHeader));
			// code 다른게 오면 끊음
			if (header.code != CLIENT_SETTING::PACKET_KEY)
			{
				CSerializableBufferView<FALSE>::Free(m_pDelayedBuffer);
				m_pDelayedBuffer = nullptr;

				if (m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);

				m_pRecvBuffer = nullptr;

				g_netClientMgr->m_arrNetClients[m_ClientMgrIndex]->Disconnect(m_uiSessionID);
				return;
			}

			if (header.len >= 65535 || header.len >= m_pRecvBuffer->GetCapacity())
			{
				CSerializableBufferView<FALSE>::Free(m_pDelayedBuffer);
				m_pDelayedBuffer = nullptr;

				if (m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);

				m_pRecvBuffer = nullptr;

				g_netClientMgr->m_arrNetClients[m_ClientMgrIndex]->Disconnect(m_uiSessionID);
				return;
			}

			int requireSize = header.len - m_pDelayedBuffer->GetDataSize();
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


				// OnRecv 호출 전 패킷 검증
				CEncryption::Decoding(m_pDelayedBuffer->GetBufferPtr() + 4, m_pDelayedBuffer->GetFullSize() - 4, header.randKey);
				BYTE dataCheckSum = CEncryption::CalCheckSum(m_pDelayedBuffer->GetContentBufferPtr(), m_pDelayedBuffer->GetDataSize());
				m_pDelayedBuffer->GetHeader((char *)&header, (int)CSerializableBuffer<FALSE>::DEFINE::HEADER_SIZE);
				if (dataCheckSum != header.checkSum) // code 다른 것도 걸러낼 것임
				{
					CSerializableBufferView<FALSE>::Free(m_pDelayedBuffer);
					m_pDelayedBuffer = nullptr;

					if (m_pRecvBuffer->DecreaseRef() == 0)
						CRecvBuffer::Free(m_pRecvBuffer);

					m_pRecvBuffer = nullptr;

					g_netClientMgr->m_arrNetClients[m_ClientMgrIndex]->Disconnect(m_uiSessionID);
					return;
				}

				InterlockedIncrement(&g_monitor.m_lRecvTPS);
				m_pDelayedBuffer->m_uiSessionId = m_uiSessionID;
				g_netClientMgr->m_arrNetClients[m_ClientMgrIndex]->OnRecv(m_uiSessionID, m_pDelayedBuffer);
				m_pDelayedBuffer = nullptr;
			}
		}

		DWORD currentUseSize = m_pRecvBuffer->GetUseSize();
		while (currentUseSize > 0)
		{
			NetHeader packetHeader;
			CSerializableBufferView<FALSE> *view = CSerializableBufferView<FALSE>::Alloc();

			if (currentUseSize < (int)CSerializableBuffer<FALSE>::DEFINE::HEADER_SIZE)
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
			m_pRecvBuffer->Peek((char *)&packetHeader, (int)CSerializableBuffer<FALSE>::DEFINE::HEADER_SIZE);
			// code 다른게 오면 끊음
			if (packetHeader.code != CLIENT_SETTING::PACKET_KEY)
			{
				CSerializableBufferView<FALSE>::Free(view);

				if (m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);

				m_pRecvBuffer = nullptr;

				g_netClientMgr->m_arrNetClients[m_ClientMgrIndex]->Disconnect(m_uiSessionID);
				return;
			}

			if (packetHeader.len >= 65535 || packetHeader.len >= m_pRecvBuffer->GetCapacity())
			{
				CSerializableBufferView<FALSE>::Free(view);

				if (m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);

				m_pRecvBuffer = nullptr;

				g_netClientMgr->m_arrNetClients[m_ClientMgrIndex]->Disconnect(m_uiSessionID);
				return;
			}

			if (useSize < (int)CSerializableBuffer<FALSE>::DEFINE::HEADER_SIZE + packetHeader.len)
			{
				// 직접 할당해서 뒤로 밀기 Delayed
				delayFlag = true;
				// 헤더는 읽은 것임
				// front 부터 끝까지 복사
				view->InitAndAlloc(packetHeader.len + sizeof(NetHeader));
				view->Copy(m_pRecvBuffer->GetFrontPtr(), 0, useSize);
				m_pRecvBuffer->MoveFront(useSize);
				m_pDelayedBuffer = view;

				// g_Logger->WriteLog(L"RecvBuffer", LOG_LEVEL::DEBUG, L"PacketDelay");
				break;
			}

			// 실질적으로 Recv 버퍼 입장에서는 읽은 것
			// - 하나의 메시지가 완전히 완성된 상태
			int offsetStart = m_pRecvBuffer->GetFrontOffset();
			int offsetEnd = offsetStart + packetHeader.len + (int)CSerializableBuffer<FALSE>::DEFINE::HEADER_SIZE;

			// 이 view를 그냥 써도 됨
			// Init 에서 RecvBuffer의 Ref가 증가
			view->Init(m_pRecvBuffer, offsetStart, offsetEnd);
			m_pRecvBuffer->MoveFront((int)CSerializableBuffer<FALSE>::DEFINE::HEADER_SIZE + packetHeader.len);

			// OnRecv 호출 전 패킷 검증
			CEncryption::Decoding(view->GetBufferPtr() + 4, view->GetFullSize() - 4, packetHeader.randKey);
			BYTE dataCheckSum = CEncryption::CalCheckSum(view->GetContentBufferPtr(), view->GetDataSize());
			view->GetHeader((char *)&packetHeader, (int)CSerializableBuffer<FALSE>::DEFINE::HEADER_SIZE);
			if (dataCheckSum != packetHeader.checkSum) // code 다른 것도 걸러낼 것임
			{
				CSerializableBufferView<FALSE>::Free(view);

				if (m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);

				m_pRecvBuffer = nullptr;

				g_netClientMgr->m_arrNetClients[m_ClientMgrIndex]->Disconnect(m_uiSessionID);
				return;
			}

			InterlockedIncrement(&g_monitor.m_lRecvTPS);
			view->m_uiSessionId = m_uiSessionID;
			g_netClientMgr->m_arrNetClients[m_ClientMgrIndex]->OnRecv(m_uiSessionID, view);

			currentUseSize = m_pRecvBuffer->GetUseSize();
		}

		if (!delayFlag)
		{
			m_pDelayedBuffer = nullptr;
		}

		// Ref 감소
		if (m_pRecvBuffer->DecreaseRef() == 0)
			CRecvBuffer::Free(m_pRecvBuffer);

		m_pRecvBuffer = nullptr;
	}

	bool CNetSession::SendPacket(CSerializableBuffer<FALSE> *message) noexcept
	{
		// 여기서 올라간 RefCount는 SendCompleted에서 내려감
		// 혹은 ReleaseSession
		message->IncreaseRef();
		m_lfSendBufferQueue.Enqueue(message);
		return TRUE;
	}

	void CNetSession::SendCompleted(int size) noexcept
	{
		InterlockedAdd(&g_monitor.m_lSendTPS, m_iSendCount);

		// m_iSendCount를 믿고 할당 해제를 진행
		// * 논블락킹 I/O일 때만 Send를 요청한 데이터보다 덜 보내는 상황이 발생 가능
		// * 비동기 I/O는 무조건 전부 보내고 완료 통지가 도착함


		int count;
		for (count = 0; count < m_iSendCount; count++)
		{
			// RefCount를 낮추고 0이라면 보낸 거 삭제
			if (m_arrPSendBufs[count]->DecreaseRef() == 0)
			{
				CSerializableBuffer<FALSE>::Free(m_arrPSendBufs[count]);
			}
		}

		m_iSendCount = 0;

		// SendFlag를 풀지 않고 진행
		PostSend(TRUE);
	}

	bool CNetSession::PostRecv() noexcept
	{
		int errVal;
		int retVal;

		// 여기서는 수동 Ref 버전이 필요
		// * WSARecv에서 Ref 관리가 불가능

		InterlockedIncrement(&m_iIOCountAndRelease);
		if ((m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
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

	bool CNetSession::PostSend(bool isPQCS) noexcept
	{
		int errVal;
		int retVal;
		int sendUseSize;

		if (!isPQCS)
		{
			sendUseSize = m_lfSendBufferQueue.GetUseSize();
			if (sendUseSize <= 0)
			{
				return FALSE;
			}


			if (InterlockedExchange(&m_iSendFlag, TRUE) == TRUE)
			{
				return FALSE;
			}
		}

		// 여기서 얻은 만큼 쓸 것
		// 플래그 획득 이후의 실패는 플래그 풀어주기
		sendUseSize = m_lfSendBufferQueue.GetUseSize();
		if (sendUseSize <= 0)
		{
			InterlockedExchange(&m_iSendFlag, FALSE);
			return FALSE;
		}

		InterlockedIncrement(&m_iIOCountAndRelease);
		if ((m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
		{
			InterlockedDecrement(&m_iIOCountAndRelease);
			InterlockedExchange(&m_iSendFlag, FALSE);
			return FALSE;
		}

		WSABUF wsaBuf[WSASEND_MAX_BUFFER_COUNT];

		m_iSendCount = min(sendUseSize, WSASEND_MAX_BUFFER_COUNT);
		// WSASEND_MAX_BUFFER_COUNT 만큼 1초에 몇번 보내는지 카운트
		// 이 수치가 높다면 더 늘릴 것
		if (m_iSendCount == WSASEND_MAX_BUFFER_COUNT)
			InterlockedIncrement(&g_monitor.m_lMaxSendCount);

		int count;
		for (count = 0; count < m_iSendCount; count++)
		{
			CSerializableBuffer<FALSE> *pBuffer;
			// 못꺼낸 것
			if (!m_lfSendBufferQueue.Dequeue(&pBuffer))
			{
				g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"LFQueue::Dequeue() Error");
				// 말도 안되는 상황
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
					g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSASend() Error : %d", errVal);

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

	// NetClient
	// 서버 연결 준비 작업
	BOOL CNetClient::Init(INT maxSessionCount) noexcept
	{
		m_iMaxSessionCount = maxSessionCount;
		// 디스커넥트 스택 채우기
		USHORT val = m_iMaxSessionCount - 1;
		for (int i = 0; i < m_iMaxSessionCount; i++)
		{
			m_stackDisconnectIndex.Push(val--);
		}

		m_arrPSessions = new CNetSession * [maxSessionCount];

		return TRUE;
	}

	void CNetClient::SendPacket(const UINT64 sessionID, CSerializableBuffer<FALSE> *sBuffer) noexcept
	{
		CNetSession *pSession = m_arrPSessions[GetIndex(sessionID)];

		InterlockedIncrement(&pSession->m_iIOCountAndRelease);
		// ReleaseFlag가 이미 켜진 상황
		if ((pSession->m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession);
			}
			return;
		}

		if (sessionID != pSession->m_uiSessionID)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession);
			}
			return;
		}

		if (!sBuffer->GetIsEnqueueHeader())
		{
			NetHeader header;
			header.code = 119; // 코드
			header.len = sBuffer->GetDataSize();
			header.randKey = rand() % 256;
			header.checkSum = CEncryption::CalCheckSum(sBuffer->GetContentBufferPtr(), sBuffer->GetDataSize());
			sBuffer->EnqueueHeader((char *)&header, sizeof(NetHeader));

			// CheckSum 부터 암호화하기 위해
			CEncryption::Encoding(sBuffer->GetBufferPtr() + 4, sBuffer->GetBufferSize() - 4, header.randKey);
		}

		pSession->SendPacket(sBuffer);
		pSession->PostSend();

		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			ReleaseSession(pSession);
		}
	}

	void CNetClient::EnqueuePacket(const UINT64 sessionID, CSerializableBuffer<FALSE> *sBuffer) noexcept
	{
		CNetSession *pSession = m_arrPSessions[GetIndex(sessionID)];

		InterlockedIncrement(&pSession->m_iIOCountAndRelease);
		// ReleaseFlag가 이미 켜진 상황
		if ((pSession->m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession, TRUE);
			}
			return;
		}

		if (sessionID != pSession->m_uiSessionID)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession, TRUE);
			}
			return;
		}

		if (!sBuffer->GetIsEnqueueHeader())
		{
			NetHeader header;
			header.code = 119; // 코드
			header.len = sBuffer->GetDataSize();
			header.randKey = rand() % 256;
			header.checkSum = CEncryption::CalCheckSum(sBuffer->GetContentBufferPtr(), sBuffer->GetDataSize());
			sBuffer->EnqueueHeader((char *)&header, sizeof(NetHeader));

			// CheckSum 부터 암호화하기 위해
			CEncryption::Encoding(sBuffer->GetBufferPtr() + 4, sBuffer->GetBufferSize() - 4, header.randKey);
		}

		pSession->SendPacket(sBuffer);

		// {
		// 	// PROFILE_BEGIN(0, "WSASend");
		// 	pSession->PostSend();
		// }

		// {
		// 	// PROFILE_BEGIN(0, "PQCS");
		// 
		// 	if (pSession->m_iSendFlag == FALSE)
		// 	{
		// 		PostQueuedCompletionStatus(m_hIOCPHandle, 0, (ULONG_PTR)pSession, (LPOVERLAPPED)IOOperation::SENDPOST);
		// 	}
		// }

		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			ReleaseSession(pSession, TRUE);
		}
	}

	BOOL CNetClient::Disconnect(const UINT64 sessionID, BOOL isPQCS) noexcept
	{
		CNetSession *pSession = m_arrPSessions[GetIndex(sessionID)];
		if (pSession == nullptr)
			return FALSE;

		// ReleaseFlag가 이미 켜진 상황
		InterlockedIncrement(&pSession->m_iIOCountAndRelease);
		if ((pSession->m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession, isPQCS);
			}
			return FALSE;
		}

		if (sessionID != pSession->m_uiSessionID)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession, isPQCS);
			}
			return FALSE;
		}

		if (InterlockedExchange(&pSession->m_iCacelIoCalled, TRUE) == TRUE)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession, isPQCS);
			}
			return FALSE;
		}

		// Io 실패 유도
		CancelIoEx((HANDLE)pSession->m_sSessionSocket, nullptr);

		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			// IOCount == 0 이면 해제 시도
			ReleaseSession(pSession, isPQCS);
		}

		return TRUE;
	}

	BOOL CNetClient::ReleaseSession(CNetSession *pSession, BOOL isPQCS) noexcept
	{
		// IoCount 체크와 ReleaseFlag 변경을 동시에
		if (InterlockedCompareExchange(&pSession->m_iIOCountAndRelease, CNetSession::RELEASE_FLAG, 0) != 0)
		{
			// ioCount 0이 아니거나 이미 Release 진행 중인게 있다면 return
			return FALSE;
		}

		if (isPQCS)
		{
			PostQueuedCompletionStatus(g_netClientMgr->m_hIOCPHandle, 0, (ULONG_PTR)pSession, (LPOVERLAPPED)IOOperation::RELEASE_SESSION);
			return TRUE;
		}

		USHORT index = GetIndex(pSession->m_uiSessionID);
		closesocket(pSession->m_sSessionSocket);
		UINT64 freeSessionId = pSession->m_uiSessionID;
		CNetSession::Free(pSession);
		InterlockedDecrement(&m_iSessionCount);

		OnDisconnect(freeSessionId);

		m_stackDisconnectIndex.Push(index);

		return TRUE;
	}

	BOOL CNetClient::ReleaseSessionPQCS(CNetSession *pSession) noexcept
	{
		USHORT index = GetIndex(pSession->m_uiSessionID);
		closesocket(pSession->m_sSessionSocket);
		UINT64 freeSessionId = pSession->m_uiSessionID;
		CNetSession::Free(pSession);
		InterlockedDecrement(&m_iSessionCount);

		OnDisconnect(freeSessionId);

		m_stackDisconnectIndex.Push(index);

		return TRUE;
	}

	BOOL CNetClient::PostConnectEx(const CHAR *ip, const USHORT port) noexcept
	{
		int retVal;
		int errVal;

		CNetSession *pConnectSession = CNetSession::Alloc();
		pConnectSession->m_sSessionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (pConnectSession->m_sSessionSocket == INVALID_SOCKET)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"PostAcceptEx socket() 실패 : %d", errVal);
			return FALSE;
		}

		// 송신 버퍼를 0으로 만들어서 실직적인 I/O를 우리의 버퍼를 이용하도록 만듬
		if (CLIENT_SETTING::USE_ZERO_COPY)
		{
			DWORD sendBufferSize = 0;
			retVal = setsockopt(pConnectSession->m_sSessionSocket, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufferSize, sizeof(DWORD));
			if (retVal == SOCKET_ERROR)
			{
				errVal = WSAGetLastError();
				g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"setsockopt(SO_SNDBUF) 실패 : %d", errVal);
				return FALSE;
			}
		}

		// LINGER option 설정
		LINGER ling{ 1, 0 };
		retVal = setsockopt(pConnectSession->m_sSessionSocket, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof(ling));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"setsockopt(SO_LINGER) 실패 : %d", errVal);
			return FALSE;
		}

		SOCKADDR_IN clientAddr;
		ZeroMemory(&clientAddr, sizeof(clientAddr));
		clientAddr.sin_family = AF_INET;
		clientAddr.sin_addr.s_addr = INADDR_ANY;
		clientAddr.sin_port = 0;

		retVal = bind(pConnectSession->m_sSessionSocket, (SOCKADDR *)&clientAddr, sizeof(clientAddr));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"bind() 실패 : %d", errVal);
			return FALSE;
		}

		// 세션 ID 생성
		USHORT index;
		// 연결 실패 : FALSE
		if (!m_stackDisconnectIndex.Pop(&index))
		{
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"m_stackDisconnectIndex.Pop(&index) failed");
			return FALSE;
		}

		UINT64 combineId = CombineIndex(index, InterlockedIncrement64(&m_iCurrentID));
		pConnectSession->Init(combineId, m_ClientMgrIndex);

		m_arrPSessions[index] = pConnectSession;

		g_netClientMgr->RegisterIoCompletionPort(pConnectSession->m_sSessionSocket, (ULONG_PTR)pConnectSession);

		ZeroMemory(pConnectSession->m_pMyOverlappedStartAddr, sizeof(OVERLAPPED));

		SOCKADDR_IN serverAddr;
		ZeroMemory(&serverAddr, sizeof(serverAddr));
		serverAddr.sin_family = AF_INET;
		InetPtonA(AF_INET, ip, &serverAddr.sin_addr);
		serverAddr.sin_port = htons(port);

		if (!g_netClientMgr->ConnectEx(pConnectSession->m_sSessionSocket, (sockaddr *)&serverAddr, sizeof(serverAddr), pConnectSession->m_pMyOverlappedStartAddr))
		{
			errVal = WSAGetLastError();
			if (errVal != ERROR_IO_PENDING)
			{
				if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET)
					g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"AcceptEx() Error : %d", errVal);

				return FALSE;
			}
		}

		return TRUE;
	}

	BOOL CNetClient::ConnectExCompleted(CNetSession *pSession) noexcept
	{
		int retVal;
		int errVal;

		// SO_UPDATE_CONNECT_CONTEXT 옵션 설정
		retVal = setsockopt(pSession->m_sSessionSocket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"setsockopt(SO_UPDATE_CONNECT_CONTEXT) 실패 : %d", errVal);
			return FALSE;
		}

		return TRUE;
	}

	unsigned int NetWorkerThreadFunc(LPVOID lpParam) noexcept
	{
		return g_netClientMgr->WorkerThread();
	}

	unsigned int NetTimerEventSchedulerThreadFunc(LPVOID lpParam) noexcept
	{
		return g_netClientMgr->TimerEventSchedulerThread();
	}

	BOOL CNetClientManager::Init() noexcept
	{
		int retVal;
		int errVal;
		WSAData wsaData;

		retVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (retVal != 0)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSAStartup() 실패 : %d", errVal);
			return FALSE;
		}

		m_hIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, CLIENT_SETTING::IOCP_ACTIVE_THREAD);
		if (m_hIOCPHandle == NULL)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"CreateIoCompletionPort(생성) 실패 : %d", errVal);
			return FALSE;
		}

		// 더미 소켓
		SOCKET dummySocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
		if (dummySocket == INVALID_SOCKET)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSASocket() 실패 : %d", errVal);
			return FALSE;
		}

		DWORD dwBytes;
		retVal = WSAIoctl(dummySocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidConnectEx, sizeof(m_guidConnectEx), &m_lpfnConnectEx, sizeof(m_lpfnConnectEx), &dwBytes, NULL, NULL);
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSAIoctl(lpfnConnectEx) 실패 : %d", errVal);
			return FALSE;
		}

		closesocket(dummySocket);

		return TRUE;
	}

	BOOL CNetClientManager::Start() noexcept
	{
		int retVal;
		int errVal;

		// CreateTimerSchedulerThread
		m_hTimerEventSchedulerThread = (HANDLE)_beginthreadex(nullptr, 0, NetTimerEventSchedulerThreadFunc, nullptr, 0, nullptr);
		if (m_hTimerEventSchedulerThread == 0)
		{
			errVal = GetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"TimerEventSchedulerThread running fail.. : %d", errVal);
			return FALSE;
		}

		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"[SYSTEM] TimerEventSchedulerThread running..");

		// CreateWorkerThread
		for (int i = 1; i <= CLIENT_SETTING::IOCP_WORKER_THREAD; i++)
		{
			HANDLE hWorkerThread = (HANDLE)_beginthreadex(nullptr, 0, NetWorkerThreadFunc, nullptr, 0, nullptr);
			if (hWorkerThread == 0)
			{
				errVal = GetLastError();
				g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WorkerThread[%d] running fail.. : %d", i, errVal);
				return FALSE;
			}

			m_arrWorkerThreads.push_back(hWorkerThread);
			g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"[SYSTEM] WorkerThread[%d] running..", i);
		}

		return TRUE;
	}

	BOOL CNetClientManager::Wait() noexcept
	{
		WaitForMultipleObjects(m_arrWorkerThreads.size(), m_arrWorkerThreads.data(), TRUE, INFINITE);
		WaitForSingleObject(m_hTimerEventSchedulerThread, INFINITE);
		return TRUE;
	}

	void CNetClientManager::RegisterIoCompletionPort(SOCKET sock, ULONG_PTR sessionID)
	{
		CreateIoCompletionPort((HANDLE)sock, m_hIOCPHandle, sessionID, 0);
	}

	int CNetClientManager::WorkerThread() noexcept
	{
		int retVal;
		DWORD flag = 0;
		while (m_bIsWorkerRun)
		{
			DWORD dwTransferred = 0;
			CNetSession *pSession = nullptr;
			OVERLAPPED *lpOverlapped = nullptr;

			retVal = GetQueuedCompletionStatus(m_hIOCPHandle, &dwTransferred
				, (PULONG_PTR)&pSession, (LPOVERLAPPED *)&lpOverlapped
				, INFINITE);

			IOOperation oper;
			if ((UINT64)lpOverlapped >= 3)
			{
				if ((UINT64)lpOverlapped < 0xFFFF)
				{
					oper = (IOOperation)(UINT64)lpOverlapped;
				}
				else
				{
					oper = g_OverlappedAlloc.CalOperation((ULONG_PTR)lpOverlapped);
				}
			}

			if (lpOverlapped == nullptr)
			{
				if (retVal == FALSE)
				{
					// GetQueuedCompletionStatus Fail
					break;
				}

				if (dwTransferred == 0 && pSession == 0)
				{
					// 정상 종료 루틴
					PostQueuedCompletionStatus(m_hIOCPHandle, 0, 0, 0);
					break;
				}
			}
			// 소켓 정상 종료
			else if (dwTransferred == 0 && oper != IOOperation::CONNECTEX && (UINT)oper < 3)
			{
				// Disconnect(pSession->m_uiSessionID);
			}
			else
			{
				switch (oper)
				{
				case IOOperation::CONNECTEX:
				{
					InterlockedIncrement(&pSession->m_iIOCountAndRelease);
					if (!m_arrNetClients[pSession->m_ClientMgrIndex]->ConnectExCompleted(pSession))
					{
						if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
							CNetSession::Free(pSession);
						break;
					}

					m_arrNetClients[pSession->m_ClientMgrIndex]->OnConnect(pSession->m_uiSessionID);
					pSession->PostRecv();

					if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
						m_arrNetClients[pSession->m_ClientMgrIndex]->ReleaseSession(pSession);

					continue;
				}
				break;
				case IOOperation::RECV:
				{
					pSession->RecvCompleted(dwTransferred);
					pSession->PostRecv();
				}
				break;
				case IOOperation::SEND:
				{
					pSession->SendCompleted(dwTransferred);
					pSession->PostSend();
				}
				break;
				case IOOperation::SENDPOST:
				{
					pSession->PostSend();
					continue;
				}
				break;
				case IOOperation::RELEASE_SESSION:
				{
					m_arrNetClients[pSession->m_ClientMgrIndex]->ReleaseSessionPQCS(pSession);
					continue;
				}
				break;
				case IOOperation::TIMER_EVENT:
				{
					TimerEvent *timerEvent = (TimerEvent *)pSession;
					timerEvent->nextExecuteTime += timerEvent->timeMs;
					timerEvent->execute();

					// Event Apc Enqueue - PQ 동기화 피하기 위함
					RegisterTimerEvent(timerEvent);
					continue;
				}
				break;
				case IOOperation::CONTENT_EVENT: // 일회성 이벤트
				{
					BaseEvent *contentEvent = (BaseEvent *)pSession;
					contentEvent->execute();
					delete contentEvent;
					continue;
				}
				break;
				}
			}

			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				m_arrNetClients[pSession->m_ClientMgrIndex]->ReleaseSession(pSession);
			}
		}

		return 0;
	}

	int CNetClientManager::TimerEventSchedulerThread() noexcept
	{
		// IOCP의 병행성 관리를 받기 위한 GQCS
		DWORD dwTransferred = 0;
		CNetSession *pSession = nullptr;
		OVERLAPPED *lpOverlapped = nullptr;

		GetQueuedCompletionStatus(m_hIOCPHandle, &dwTransferred
			, (PULONG_PTR)&pSession, (LPOVERLAPPED *)&lpOverlapped
			, 0);


		while (m_bIsTimerEventSchedulerRun)
		{
			if (m_TimerEventQueue.empty())
				SleepEx(INFINITE, TRUE); // APC 작업이 Enqueue 되면 깨어날 것

			// EventQueue에 무언가 있을 것
			TimerEvent *timerEvent = m_TimerEventQueue.top();
			// 수행 가능 여부 판단
			LONG dTime = timerEvent->nextExecuteTime - timeGetTime();
			if (dTime <= 0) // 0보다 작으면 수행 가능
			{
				m_TimerEventQueue.pop();
				PostQueuedCompletionStatus(m_hIOCPHandle, 0, (ULONG_PTR)timerEvent, (LPOVERLAPPED)IOOperation::TIMER_EVENT);
				continue;
			}

			// 남은 시간만큼 블록
			SleepEx(dTime, TRUE);
		}

		return 0;
	}

	void CNetClientManager::RegisterTimerEvent(BaseEvent *timerEvent) noexcept
	{
		int retVal;
		int errVal;

		retVal = QueueUserAPC(RegisterTimerEventAPCFunc, m_hTimerEventSchedulerThread, (ULONG_PTR)timerEvent);
		if (retVal == FALSE)
		{
			errVal = GetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"QueueUserAPC() 실패 : %d", errVal);
		}
	}

	// EventSchedulerThread에서 수행됨
	void CNetClientManager::RegisterTimerEventAPCFunc(ULONG_PTR lpParam) noexcept
	{
		g_netClientMgr->m_TimerEventQueue.push((TimerEvent *)lpParam);
	}
	void CNetClientManager::ContentEventEnqueue(BaseEvent *event) noexcept
	{
		PostQueuedCompletionStatus(m_hIOCPHandle, 0, (ULONG_PTR)event, (LPOVERLAPPED)IOOperation::CONTENT_EVENT);
	}
}
