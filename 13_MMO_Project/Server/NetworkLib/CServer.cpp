#include "ServerNetworkLibPch.h"
#include "SNetworkCounters.h"


namespace ServerNetworkLib
{
CServer *g_pServer = nullptr;

	unsigned int NetWorkerThreadFunc(LPVOID pParam) noexcept
	{
		g_pLogger->RegisterLogProducer();
		return g_pServer->WorkerThread();
	}

	BOOL CServer::Start(const SServerNetworkSettings &settings) noexcept
	{
		if (settings.packetMode != EPacketMode::Net && settings.packetMode != EPacketMode::Lan)
			return FALSE;

		m_packetCode = settings.packetCode;
		m_packetMode = settings.packetMode;
		m_maxSessionCount = settings.maxSessionCount;
		m_acceptExCount = settings.acceptExCount;
		int retVal;
		int errVal;
		WSAData wsaData;

		m_pSessions = new CSession * [m_maxSessionCount];

		ZeroMemory((char *)m_pSessions, sizeof(CSession *) * m_maxSessionCount);

		m_pAcceptExSessions = new CSession * [m_acceptExCount];

		// 디스커넥트 스택 채우기
		USHORT val = m_maxSessionCount - 1;
		for (int i = 0; i < m_maxSessionCount; i++)
		{
			m_stackDisconnectIndex.Push(val--);
		}

		retVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (retVal != 0)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=winsock_start_failed error_code=%d | Winsock 초기화에 실패했습니다.", errVal);
			return FALSE;
		}

		m_listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
		if (m_listenSocket == INVALID_SOCKET)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=listen_socket_create_failed error_code=%d | 수신 대기 소켓을 생성하지 못했습니다.", errVal);
			return FALSE;
		}

		SOCKADDR_IN serverAddr;
		ZeroMemory(&serverAddr, sizeof(serverAddr));
		serverAddr.sin_family = AF_INET;
		InetPtonA(AF_INET, settings.ip.c_str(), &serverAddr.sin_addr);
		serverAddr.sin_port = htons(settings.port);

		retVal = bind(m_listenSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=bind_failed error_code=%d | 서버 소켓 바인딩에 실패했습니다.", errVal);
			return FALSE;
		}

		// 송신 버퍼를 0으로 만들어서 실직적인 I/O를 우리의 버퍼를 이용하도록 만듬
		if (settings.useZeroCopy)
		{
			DWORD sendBufferSize = 0;
			retVal = setsockopt(m_listenSocket, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufferSize, sizeof(DWORD));
			if (retVal == SOCKET_ERROR)
			{
				errVal = WSAGetLastError();
				g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
					L"event=send_buffer_set_failed error_code=%d | 소켓 송신 버퍼 설정에 실패했습니다.", errVal);
				return FALSE;
			}
		}

		// LINGER option 설정
		LINGER ling{ 1, 0 };
		retVal = setsockopt(m_listenSocket, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof(ling));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=linger_set_failed error_code=%d | 소켓 종료 대기 옵션 설정에 실패했습니다.", errVal);
			return FALSE;
		}

		// listen
		retVal = listen(m_listenSocket, SOMAXCONN_HINT(65535));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=listen_failed error_code=%d | 소켓 수신 대기를 시작하지 못했습니다.", errVal);
			return FALSE;
		}

		// CP 핸들 생성
		m_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, settings.iocpConcurrency);
		if (m_iocpHandle == NULL)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=iocp_create_failed error_code=%d | I/O 완료 포트를 생성하지 못했습니다.", errVal);
			return FALSE;
		}

		CreateIoCompletionPort((HANDLE)m_listenSocket, m_iocpHandle, (ULONG_PTR)0, 0);

		DWORD dwBytes;
		retVal = WSAIoctl(m_listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidAcceptEx, sizeof(m_guidAcceptEx),
			&m_pAcceptEx, sizeof(m_pAcceptEx), &dwBytes, NULL, NULL);
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=accept_ex_lookup_failed error_code=%d | AcceptEx 함수 조회에 실패했습니다.", errVal);
			return FALSE;
		}

		retVal = WSAIoctl(m_listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidGetAcceptExSockaddrs,
			sizeof(m_guidGetAcceptExSockaddrs), &m_pGetAcceptExSockaddrs, sizeof(m_pGetAcceptExSockaddrs), &dwBytes, NULL, NULL);
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=accept_addr_lookup_failed error_code=%d | GetAcceptExSockaddrs 함수 조회에 실패했습니다.", errVal);
			return FALSE;
		}

		// AcceptEx 요청
		FristPostAcceptEx();

		m_hasStartedBackgroundWork = true;

		// CreateWorkerThread
		for (int i = 1; i <= settings.iocpWorkerCount; i++)
		{
			HANDLE hWorkerThread = (HANDLE)_beginthreadex(nullptr, 0, NetWorkerThreadFunc, nullptr, 0, nullptr);
			if (hWorkerThread == 0)
			{
				const int workerError = errno;
				g_pLogger->WriteEmergencyLogToStderr(
					"[Error][NetworkLib] event=worker_start_failed worker_index=%d error_code=%d | 네트워크 워커를 시작하지 못했습니다.\n",
					i, workerError);
				return FALSE;
			}

			m_arrWorkerThreads.push_back(hWorkerThread);
			g_pLogger->WriteLog(ELogLevel::Info, L"NetworkLib", 0, std::source_location::current(),
				L"event=worker_started worker_index=%d | 네트워크 워커를 시작했습니다.", i);
		}

		OnServerReady(settings.port);

		WaitForMultipleObjects((DWORD)m_arrWorkerThreads.size(), m_arrWorkerThreads.data(), TRUE, INFINITE);

		return TRUE;
	}

	void CServer::Stop()
	{
		// listen 소켓 닫기
		InterlockedExchange(&m_isStop, TRUE);
		closesocket(m_listenSocket);

		for (int i = 0; i < m_maxSessionCount; i++)
		{
			if (m_pSessions[i] != NULL)
			{
				Disconnect(m_pSessions[i]->m_sessionId);
			}
		}

		// while SessionCount 0일 때까지
		while (true)
		{
			// 세션 전부 끊고
			if (GetSessionCount() == 0)
			{
				m_isWorkerRun = FALSE;
				PostQueuedCompletionStatus(m_iocpHandle, 0, 0, 0);
				break;
			}
		}
	}

	bool CServer::TryPopRecvPacket(UINT64 sessionId, RecvPacketPtr &outPacket) noexcept
	{
		outPacket.reset();
		const USHORT index = getIndex(sessionId);
		if (m_pSessions == nullptr || index >= m_maxSessionCount)
			return false;
		CSession *pSession = m_pSessions[index];
		if (pSession == nullptr)
			return false;

		const LONG ioCount = InterlockedIncrement(&pSession->m_ioCountAndRelease);
		if ((ioCount & CSession::sm_releaseFlag) == 0 && pSession->m_sessionId == sessionId)
		{
			CSerializableBufferView *pView = nullptr;
			if (pSession->m_recvMsgQueue.Dequeue(&pView))
				outPacket = RecvPacketPtr(pView, CSerializableBufferView::Free);
		}

		if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
			ReleaseSession(pSession);
		return outPacket != nullptr;
	}

	void CServer::SendPacket(const UINT64 sessionId, CSerializableBuffer *pSBuffer) noexcept
	{
		CSession *pSession = m_pSessions[getIndex(sessionId)];

		InterlockedIncrement(&pSession->m_ioCountAndRelease);
		if ((pSession->m_ioCountAndRelease & CSession::sm_releaseFlag) == CSession::sm_releaseFlag)
		{
			if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
			{
				ReleaseSession(pSession);
			}
			return;
		}

		if (sessionId != pSession->m_sessionId)
		{
			if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
			{
				ReleaseSession(pSession);
			}
			return;
		}

		if (pSession->SendPacket(pSBuffer))
			pSession->PostSend();
		else
			Disconnect(sessionId);

		if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
		{
			ReleaseSession(pSession);
		}
	}

	BOOL CServer::Disconnect(const UINT64 sessionId, BOOL isPqcs) noexcept
	{
		CSession *pSession = m_pSessions[getIndex(sessionId)];

		// ReleaseFlag가 이미 켜진 상황
		InterlockedIncrement(&pSession->m_ioCountAndRelease);
		if ((pSession->m_ioCountAndRelease & CSession::sm_releaseFlag) == CSession::sm_releaseFlag)
		{
			if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
			{
				ReleaseSession(pSession, isPqcs);
			}
			return FALSE;
		}

		if (sessionId != pSession->m_sessionId)
		{
			if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
			{
				ReleaseSession(pSession, isPqcs);
			}
			return FALSE;
		}

		if (InterlockedExchange(&pSession->m_cancelIoCalled, TRUE) == TRUE)
		{
			if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
			{
				ReleaseSession(pSession, isPqcs);
			}
			return FALSE;
		}

		// Io 실패 유도
		CancelIoEx((HANDLE)pSession->m_sessionSocket, nullptr);

		if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
		{
			// IOCount == 0 이면 해제 시도
			ReleaseSession(pSession, isPqcs);
		}

		return TRUE;
	}

	BOOL CServer::ReleaseSession(CSession *pSession, BOOL isPqcs) noexcept
	{
		if (InterlockedCompareExchange(&pSession->m_ioCountAndRelease, CSession::sm_releaseFlag, 0) != 0)
		{
			return FALSE;
		}

		if (isPqcs)
		{
			PostQueuedCompletionStatus(m_iocpHandle, 0
				, (ULONG_PTR)pSession, (LPOVERLAPPED)EIoOperation::ReleaseSession);
			return TRUE;
		}

		closesocket(pSession->m_sessionSocket);
		USHORT index = getIndex(pSession->m_sessionId);
		UINT64 freeSessionId = pSession->m_sessionId;
	
		OnClientLeave(freeSessionId);

		CSession::Free(pSession);
		InterlockedDecrement(&m_sessionCount);
		m_stackDisconnectIndex.Push(index);

		return TRUE;
	}

	BOOL CServer::ReleaseSessionPqcs(CSession *pSession) noexcept
	{
		USHORT index = getIndex(pSession->m_sessionId);
		closesocket(pSession->m_sessionSocket);
		UINT64 freeSessionId = pSession->m_sessionId;

		OnClientLeave(freeSessionId);

		CSession::Free(pSession);
		InterlockedDecrement(&m_sessionCount);
		m_stackDisconnectIndex.Push(index);

		return TRUE;
	}

	void CServer::FristPostAcceptEx() noexcept
	{
		// 처음에 AcceptEx를 걸어두고 시작
		for (int i = 0; i < m_acceptExCount; i++)
		{
			PostAcceptEx(i);
		}
	}

	BOOL CServer::PostAcceptEx(INT index) noexcept
	{
		int retVal;
		int errVal;

		CSession *pNewAcceptEx = CSession::Alloc();
		m_pAcceptExSessions[index] = pNewAcceptEx;

		pNewAcceptEx->m_sessionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (pNewAcceptEx->m_sessionSocket == INVALID_SOCKET)
		{
			errVal = WSAGetLastError();
			CSession::Free(pNewAcceptEx);
			if (m_isStop)
				return FALSE;

			// 자원 오류 등: 반납만 하고 슬롯은 포기한다.
			const LONG lostCount = InterlockedIncrement(&m_lostAcceptSlotCount);
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=accept_slot_lost index=%d error_code=%d remaining=%d total=%d | 접속 수락 슬롯을 다시 걸지 못했습니다.",
				index, errVal, m_acceptExCount - lostCount, m_acceptExCount);
			return FALSE;
		}

		ZeroMemory(pNewAcceptEx->m_pMyOverlappedStartAddr, sizeof(OVERLAPPED));
		g_overlappedAllocator.SetAcceptExIndex((ULONG_PTR)pNewAcceptEx->m_pMyOverlappedStartAddr, index);

		retVal = m_pAcceptEx(m_listenSocket, pNewAcceptEx->m_sessionSocket
			, pNewAcceptEx->m_acceptBuffer, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, NULL,
			pNewAcceptEx->m_pMyOverlappedStartAddr);
		if (retVal == FALSE)
		{
			errVal = WSAGetLastError();
			if (errVal != WSA_IO_PENDING)
			{
				// 즉시 실패는 완료 통지가 오지 않으므로 여기서 반납한다.
				closesocket(pNewAcceptEx->m_sessionSocket);
				CSession::Free(pNewAcceptEx);

				// 중단 중에는 리슨 소켓이 닫혀 실패하는 것이 정상이다.
				if (m_isStop)
					return FALSE;

				// 상대가 수락 전에 끊은 연결 : 같은 슬롯에 다시 건다.
				if (errVal == WSAECONNRESET || errVal == WSAECONNABORTED)
					return PostAcceptEx(index);

				// 리슨 소켓·버퍼·인자를 잘못 넘긴 코드 버그
				if (errVal == WSAEINVAL || errVal == WSAENOTSOCK || errVal == WSAEFAULT || errVal == WSAEOPNOTSUPP)
					__debugbreak();

				// 자원 오류 등: 반납만 하고 슬롯은 포기한다.
				const LONG lostCount = InterlockedIncrement(&m_lostAcceptSlotCount);
				g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
					L"event=accept_slot_lost index=%d error_code=%d remaining=%d total=%d | 접속 수락 슬롯을 다시 걸지 못했습니다.",
					index, errVal, m_acceptExCount - lostCount, m_acceptExCount);
				return FALSE;
			}
		}

		return TRUE;
	}

	BOOL CServer::AcceptExCompleted(CSession *pSession) noexcept
	{
		InterlockedIncrement(&g_networkCounters.m_acceptTps);

		int retVal;
		int errVal;
		retVal = setsockopt(pSession->m_sessionSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
			(char *)&m_listenSocket, sizeof(SOCKET));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();

			if (!m_isStop)
				g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
					L"event=accept_context_update_failed error_code=%d | 수락 소켓의 컨텍스트 갱신에 실패했습니다.", errVal);
			return FALSE;
		}

		// 성공한 소켓에 대해 IOCP 등록
		CreateIoCompletionPort((HANDLE)pSession->m_sessionSocket, m_iocpHandle, (ULONG_PTR)pSession, 0);

		SOCKADDR_IN *pLocalAddr = nullptr;
		INT localAddrLen;

		SOCKADDR_IN *pRemoteAddr = nullptr;
		INT remoteAddrLen;
		m_pGetAcceptExSockaddrs(pSession->m_acceptBuffer, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
			(SOCKADDR **)&pLocalAddr, &localAddrLen, (SOCKADDR **)&pRemoteAddr, &remoteAddrLen);

		InetNtop(AF_INET, &pRemoteAddr->sin_addr, pSession->m_clientAddrBuffer, 16);

		pSession->m_clientPort = pRemoteAddr->sin_port;

		// TODO - 끊어줄 방법 고민
		if (!OnConnectionRequest(pSession->m_clientAddrBuffer, pSession->m_clientPort))
			return FALSE;

		USHORT index;
		// 연결 실패 : FALSE
		if (!m_stackDisconnectIndex.Pop(&index))
		{
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=session_slot_pop_failed | 비어 있는 세션 슬롯을 꺼내지 못했습니다.");
			return FALSE;
		}

		UINT64 combineId = CServer::combineIndex(index, InterlockedIncrement64(&m_currentId));

		pSession->Init(combineId);

		InterlockedIncrement(&m_sessionCount);
		InterlockedIncrement64(&g_networkCounters.m_acceptTotal);
		m_pSessions[index] = pSession;

		// 서버 중단 상태면 연결 끊기
		// * 이미 등록·카운트된 세션이므로 TRUE를 반환해 정상 해제 경로(OnAccept → 수신 취소 → ReleaseSession)로 보낸다.
		if (m_isStop)
		{
			Disconnect(combineId);
			return TRUE;
		}

		return TRUE;
	}

	int CServer::WorkerThread() noexcept
	{
		int retVal;
		DWORD flag = 0;
		while (m_isWorkerRun)
		{
			DWORD dwTransferred = 0;
			CSession *pSession = nullptr;
			OVERLAPPED *pLpOverlapped = nullptr;

			retVal = GetQueuedCompletionStatus(m_iocpHandle, &dwTransferred
				, (PULONG_PTR)&pSession, (LPOVERLAPPED *)&pLpOverlapped
				, INFINITE);


			EIoOperation oper;
			if ((UINT64)pLpOverlapped >= 3)
			{
				if ((UINT64)pLpOverlapped < 0xFFFF)
				{
					oper = (EIoOperation)(UINT64)pLpOverlapped;
				}
				else
				{
					oper = g_overlappedAllocator.CalOperation((ULONG_PTR)pLpOverlapped);
				}
			}

			if (pLpOverlapped == nullptr)
			{
				if (retVal == FALSE)
				{
					const DWORD errorCode = GetLastError();
					g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
						L"event=iocp_wait_failed error_code=%lu | I/O 완료 대기에 실패해 워커를 종료합니다.", errorCode);
					break;
				}

				if (dwTransferred == 0 && pSession == 0)
				{
					// 정상 종료 루틴
					PostQueuedCompletionStatus(m_iocpHandle, 0, 0, 0);
					break;
				}
			}
			// 수신 종료 통지 뒤에는 OS가 페이지를 더 이상 사용하지 않는다.
			else if (oper == EIoOperation::Recv && (retVal == FALSE || dwTransferred == 0))
			{
				if (pSession->m_pRecvBuffer != nullptr && pSession->m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(pSession->m_pRecvBuffer);
				pSession->m_pRecvBuffer = nullptr;

				if (pSession->m_pDelayedBuffer != nullptr)
				{
					CSerializableBufferView::Free(pSession->m_pDelayedBuffer);
					pSession->m_pDelayedBuffer = nullptr;
				}
			}
			// 소켓 정상 종료
			else if (dwTransferred == 0 && oper != EIoOperation::AcceptEx && (UINT)oper < 3)
			{
				// Disconnect(pSession->m_uiSessionID);
			}
			else
			{
				switch (oper)
				{
				case EIoOperation::AcceptEx:
				{
					// Accept가 성공한 세션 포인터를 얻어옴
					INT index = g_overlappedAllocator.GetAcceptExIndex((ULONG_PTR)pLpOverlapped);
					pSession = m_pAcceptExSessions[index];

					// 이거 실패하면 연결 끊음
					// * 실패 가능한 상황 - setsockopt, OnConnectionRequest
					// * ioCount 무조건 1일 것임
					// * 바로 끊어도 괜춘
					// * 다른 I/O 요청을 안걸고 끝내기 때문에 아래의 ioCount 0이 됨으로 연결 끊김을 유도
					InterlockedIncrement(&pSession->m_ioCountAndRelease);
					if (!AcceptExCompleted(pSession))
					{
						// 실패한 인덱스에 대한 예약은 다시 걸어줌
						if (!m_isStop)
							PostAcceptEx(index);
						
						InterlockedDecrement(&pSession->m_ioCountAndRelease);
						closesocket(pSession->m_sessionSocket);
						CSession::Free(pSession);
						continue;
					}

					// OnAccept 처리는 여기서
					OnAccept(pSession->m_sessionId);
					// 해당 세션에 대해 Recv 예약
					pSession->PostRecv();

					if (!m_isStop)
						PostAcceptEx(index);

					break;
				}
				break;
				case EIoOperation::Recv:
				{
					if (pSession->RecvCompleted(dwTransferred))
						pSession->PostRecv();
					else
						Disconnect(pSession->m_sessionId);
				}
				break;
				case EIoOperation::Send:
				{
					pSession->SendCompleted(dwTransferred);
				}
				break;
				case EIoOperation::ReleaseSession:
				{
					ReleaseSessionPqcs(pSession);
					continue;
				}
				break;
				}
			}

			if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
			{
				ReleaseSession(pSession);
			}
		}

		return 0;
	}
}
