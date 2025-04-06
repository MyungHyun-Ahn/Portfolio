#include "pch.h"
#include "ChatSetting.h"
#include "ServerSetting.h"
#include "CNetServer.h"
#include "CNetSession.h"
#include "SystemEvent.h"

CNetServer *g_NetServer = nullptr;

unsigned int NetWorkerThreadFunc(LPVOID lpParam) noexcept
{
	return g_NetServer->WorkerThread();
}

unsigned int NetTimerEventSchedulerThreadFunc(LPVOID lpParam) noexcept
{
	return g_NetServer->TimerEventSchedulerThread();
}

BOOL CNetServer::Start(const CHAR *openIP, const USHORT port) noexcept
{
	int retVal;
	int errVal;
	WSAData wsaData;

	m_usMaxSessionCount = MAX_SESSION_COUNT;

	m_arrPSessions = new CNetSession * [MAX_SESSION_COUNT];

	ZeroMemory((char *)m_arrPSessions, sizeof(CNetSession *) * MAX_SESSION_COUNT);

	m_arrAcceptExSessions = new CNetSession * [ACCEPTEX_COUNT];

	// 디스커넥트 스택 채우기
	USHORT val = MAX_SESSION_COUNT - 1;
	for (int i = 0; i < MAX_SESSION_COUNT; i++)
	{
		m_stackDisconnectIndex.Push(val--);
	}

	retVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (retVal != 0)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSAStartup() 실패 : %d", errVal);
		return FALSE;
	}

	m_sListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
	if (m_sListenSocket == INVALID_SOCKET)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSASocket() 실패 : %d", errVal);
		return FALSE;
	}

	SOCKADDR_IN serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	InetPtonA(AF_INET, openIP, &serverAddr.sin_addr);
	serverAddr.sin_port = htons(port);

	retVal = bind(m_sListenSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"bind() 실패 : %d", errVal);
		return FALSE;
	}

	// 송신 버퍼를 0으로 만들어서 실직적인 I/O를 우리의 버퍼를 이용하도록 만듬
	if (USE_ZERO_COPY)
	{
		DWORD sendBufferSize = 0;
		retVal = setsockopt(m_sListenSocket, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufferSize, sizeof(DWORD));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"setsockopt(SO_SNDBUF) 실패 : %d", errVal);
			return FALSE;
		}
	}

	// LINGER option 설정
	LINGER ling{ 1, 0 };
	retVal = setsockopt(m_sListenSocket, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof(ling));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"setsockopt(SO_LINGER) 실패 : %d", errVal);
		return FALSE;
	}

	// listen
	retVal = listen(m_sListenSocket, SOMAXCONN_HINT(65535));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"listen() 실패 : %d", errVal);
		return FALSE;
	}

	// CP 핸들 생성
	m_hIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, IOCP_ACTIVE_THREAD);
	if (m_hIOCPHandle == NULL)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"CreateIoCompletionPort(생성) 실패 : %d", errVal);
		return FALSE;
	}

	CreateIoCompletionPort((HANDLE)m_sListenSocket, m_hIOCPHandle, (ULONG_PTR)0, 0);

	DWORD dwBytes;
	retVal = WSAIoctl(m_sListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidAcceptEx, sizeof(m_guidAcceptEx), &m_lpfnAcceptEx, sizeof(m_lpfnAcceptEx), &dwBytes, NULL, NULL);
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSAIoctl(lpfnAcceptEx) 실패 : %d", errVal);
		return FALSE;
	}

	retVal = WSAIoctl(m_sListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidGetAcceptExSockaddrs, sizeof(m_guidGetAcceptExSockaddrs), &m_lpfnGetAcceptExSockaddrs, sizeof(m_lpfnGetAcceptExSockaddrs), &dwBytes, NULL, NULL);
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSAIoctl(lpfnGetAcceptExSockaddrs) 실패 : %d", errVal);
		return FALSE;
	}

	// AcceptEx 요청
	FristPostAcceptEx();

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
	for (int i = 1; i <= IOCP_WORKER_THREAD; i++)
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

	RegisterSystemTimerEvent();
	RegisterContentTimerEvent();

	WaitForMultipleObjects(m_arrWorkerThreads.size(), m_arrWorkerThreads.data(), TRUE, INFINITE);
	WaitForSingleObject(m_hTimerEventSchedulerThread, INFINITE);

	return TRUE;
}

void CNetServer::Stop()
{
	// listen 소켓 닫기
	InterlockedExchange(&m_isStop, TRUE);
	closesocket(m_sListenSocket);

	for (int i = 0; i < MAX_SESSION_COUNT; i++)
	{
		if (m_arrPSessions[i] != NULL)
		{
			Disconnect(m_arrPSessions[i]->m_uiSessionID);
		}
	}

	// while SessionCount 0일 때까지
	while (true)
	{
		// 세션 전부 끊고
		if (m_iSessionCount == 0)
		{
			m_bIsWorkerRun = FALSE;
			PostQueuedCompletionStatus(m_hIOCPHandle, 0, 0, 0);
			break;
		}
	}

	m_bIsTimerEventSchedulerRun = FALSE;
}

void CNetServer::SendPacket(const UINT64 sessionID, CSerializableBuffer<FALSE> *sBuffer) noexcept
{
	CNetSession *pSession = m_arrPSessions[GetIndex(sessionID)];

	InterlockedIncrement(&pSession->m_iIOCountAndRelease);
	if (sessionID != pSession->m_uiSessionID)
	{
		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			ReleaseSession(pSession);
		}
		return;
	}

	// ReleaseFlag가 이미 켜진 상황
	if ((pSession->m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
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
		header.code = PACKET_KEY; // 코드
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

// Sector 락이 잡힌 경우에만 PQCS
void CNetServer::EnqueuePacket(const UINT64 sessionID, CSerializableBuffer<FALSE> *sBuffer) noexcept
{
	CNetSession *pSession = m_arrPSessions[GetIndex(sessionID)];

	InterlockedIncrement(&pSession->m_iIOCountAndRelease);
	if (sessionID != pSession->m_uiSessionID)
	{
		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			ReleaseSession(pSession, TRUE);
		}
		return;
	}

	// ReleaseFlag가 이미 켜진 상황
	if ((pSession->m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
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
		header.code = PACKET_KEY; // 코드
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

BOOL CNetServer::Disconnect(const UINT64 sessionID, BOOL isPQCS) noexcept
{
	CNetSession *pSession = m_arrPSessions[GetIndex(sessionID)];
	if (pSession == nullptr)
		return FALSE;

	InterlockedIncrement(&pSession->m_iIOCountAndRelease);
	if (sessionID != pSession->m_uiSessionID)
	{
		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			ReleaseSession(pSession, isPQCS);
		}
		return FALSE;
	}

	if ((pSession->m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
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

BOOL CNetServer::ReleaseSession(CNetSession *pSession, BOOL isPQCS) noexcept
{
	// IoCount 체크와 ReleaseFlag 변경을 동시에
	if (InterlockedCompareExchange(&pSession->m_iIOCountAndRelease, CNetSession::RELEASE_FLAG, 0) != 0)
	{
		// ioCount 0이 아니거나 이미 Release 진행 중인게 있다면 return
		return FALSE;
	}

	if (isPQCS)
	{
		PostQueuedCompletionStatus(m_hIOCPHandle, 0, (ULONG_PTR)pSession, (LPOVERLAPPED)IOOperation::RELEASE_SESSION);
		return TRUE;
	}

	USHORT index = GetIndex(pSession->m_uiSessionID);
	closesocket(pSession->m_sSessionSocket);

	UINT64 freeSessionId = pSession->m_uiSessionID;
	OnClientLeave(freeSessionId);
	CNetSession::Free(pSession);
	InterlockedDecrement(&m_iSessionCount);

	m_stackDisconnectIndex.Push(index);

	return TRUE;
}

BOOL CNetServer::ReleaseSessionPQCS(CNetSession *pSession) noexcept
{
	USHORT index = GetIndex(pSession->m_uiSessionID);
	closesocket(pSession->m_sSessionSocket);
	UINT64 freeSessionId = pSession->m_uiSessionID;
	OnClientLeave(freeSessionId);
	CNetSession::Free(pSession);
	InterlockedDecrement(&m_iSessionCount);

	m_stackDisconnectIndex.Push(index);

	return TRUE;
}

void CNetServer::FristPostAcceptEx() noexcept
{
	// 처음에 AcceptEx를 걸어두고 시작
	for (int i = 0; i < ACCEPTEX_COUNT; i++)
	{
		PostAcceptEx(i);
	}
}

BOOL CNetServer::PostAcceptEx(INT index) noexcept
{
	int retVal;
	int errVal;

	CNetSession *newAcceptEx = CNetSession::Alloc();
	m_arrAcceptExSessions[index] = newAcceptEx;

	newAcceptEx->m_sSessionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (newAcceptEx->m_sSessionSocket == INVALID_SOCKET)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"PostAcceptEx socket() 실패 : %d", errVal);
		return FALSE;
	}

	ZeroMemory(newAcceptEx->m_pMyOverlappedStartAddr, sizeof(OVERLAPPED));
	g_OverlappedAlloc.SetAcceptExIndex((ULONG_PTR)newAcceptEx->m_pMyOverlappedStartAddr, index);

	retVal = m_lpfnAcceptEx(m_sListenSocket, newAcceptEx->m_sSessionSocket
		, newAcceptEx->m_AcceptBuffer, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, NULL, newAcceptEx->m_pMyOverlappedStartAddr);
	if (retVal == FALSE)
	{
		errVal = WSAGetLastError();
		if (errVal != WSA_IO_PENDING)
		{
			if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET)
				g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"AcceptEx() Error : %d", errVal);

			return FALSE;
		}
	}

	return TRUE;
}

BOOL CNetServer::AcceptExCompleted(CNetSession *pSession) noexcept
{
	InterlockedIncrement(&g_monitor.m_lAcceptTPS);

	int retVal;
	int errVal;
	retVal = setsockopt(pSession->m_sSessionSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
		(char *)&m_sListenSocket, sizeof(SOCKET));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();

		if (!m_isStop)
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"setsockopt(SO_UPDATE_ACCEPT_CONTEXT) 실패 : %d", errVal);
		return FALSE;
	}

	// 성공한 소켓에 대해 IOCP 등록
	CreateIoCompletionPort((HANDLE)pSession->m_sSessionSocket, m_hIOCPHandle, (ULONG_PTR)pSession, 0);

	SOCKADDR_IN *localAddr = nullptr;
	INT localAddrLen;

	SOCKADDR_IN *remoteAddr = nullptr;
	INT remoteAddrLen;
	m_lpfnGetAcceptExSockaddrs(pSession->m_AcceptBuffer, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, (SOCKADDR **)&localAddr, &localAddrLen, (SOCKADDR **)&remoteAddr, &remoteAddrLen);

	InetNtop(AF_INET, &remoteAddr->sin_addr, pSession->m_ClientAddrBuffer, 16);

	pSession->m_ClientPort = remoteAddr->sin_port;

	// TODO - 끊어줄 방법 고민
	if (!OnConnectionRequest(pSession->m_ClientAddrBuffer, pSession->m_ClientPort))
		return FALSE;

	USHORT index;
	// 연결 실패 : FALSE
	if (!m_stackDisconnectIndex.Pop(&index))
	{
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"m_stackDisconnectIndex.Pop(&index) failed");
		return FALSE;
	}

	UINT64 combineId = CNetServer::CombineIndex(index, InterlockedIncrement64(&m_iCurrentID));

	pSession->Init(combineId);

	InterlockedIncrement(&m_iSessionCount);
	InterlockedIncrement64(&g_monitor.m_lAcceptTotal);
	m_arrPSessions[index] = pSession;

	// 서버 중단 상태면 연결 끊기
	if (m_isStop)
	{
		Disconnect(combineId);
		return FALSE;
	}

	return TRUE;
}

int CNetServer::WorkerThread() noexcept
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
		else if (dwTransferred == 0 && oper != IOOperation::ACCEPTEX && (UINT)oper < 3)
		{
			// Disconnect(pSession->m_uiSessionID);
		}
		else
		{
			switch (oper)
			{
			case IOOperation::ACCEPTEX:
			{
				// Accept가 성공한 세션 포인터를 얻어옴
				INT index = g_OverlappedAlloc.GetAcceptExIndex((ULONG_PTR)lpOverlapped);
				pSession = m_arrAcceptExSessions[index];

				// 이거 실패하면 연결 끊음
				// * 실패 가능한 상황 - setsockopt, OnConnectionRequest
				// * ioCount 무조건 1일 것임
				// * 바로 끊어도 괜춘
				// * 다른 I/O 요청을 안걸고 끝내기 때문에 아래의 ioCount 0이 됨으로 연결 끊김을 유도
				InterlockedIncrement(&pSession->m_iIOCountAndRelease);
				if (!AcceptExCompleted(pSession))
				{
					// 실패한 인덱스에 대한 예약은 다시 걸어줌
					if (!m_isStop)
						PostAcceptEx(index);
					else
						if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
							CNetSession::Free(pSession);
					break;
				}

				// OnAccept 처리는 여기서
				OnAccept(pSession->m_uiSessionID);
				// 해당 세션에 대해 Recv 예약
				pSession->PostRecv();

				if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
					ReleaseSession(pSession);

				if (!m_isStop)
					PostAcceptEx(index);

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
				ReleaseSessionPQCS(pSession);
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
			ReleaseSession(pSession);
		}
	}

	return 0;
}

// SendFrameThread로 활용
int CNetServer::TimerEventSchedulerThread() noexcept
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

void CNetServer::RegisterSystemTimerEvent()
{
	MonitorTimerEvent *monitorEvent = new MonitorTimerEvent;
	monitorEvent->SetEvent();
	RegisterTimerEvent((BaseEvent *)monitorEvent);

	KeyBoardTimerEvent *keyBoardEvent = new KeyBoardTimerEvent;
	keyBoardEvent->SetEvent();
	RegisterTimerEvent((BaseEvent *)keyBoardEvent);
}

void CNetServer::RegisterTimerEvent(BaseEvent *timerEvent) noexcept
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
void CNetServer::RegisterTimerEventAPCFunc(ULONG_PTR lpParam) noexcept
{
	g_NetServer->m_TimerEventQueue.push((TimerEvent *)lpParam);
}
