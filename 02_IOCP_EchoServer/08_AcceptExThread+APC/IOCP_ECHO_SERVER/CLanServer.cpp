#include "pch.h"
#include "ServerConfig.h"
#include "CLanServer.h"
#include "CSession.h"

CLanServer *g_Server;

unsigned int WorkerThreadFunc(LPVOID lpParam)
{
	return g_Server->WorkerThread();
}

unsigned int PostAcceptThreadFunc(LPVOID lpParam)
{
	return g_Server->PostAcceptThread();
}

BOOL CLanServer::Start(const CHAR *openIP, const USHORT port, USHORT createWorkerThreadCount, USHORT maxWorkerThreadCount, INT maxSessionCount)
{
	InitializeSRWLock(&m_disconnectStackLock);
	InitializeSRWLock(&m_freeAcceptExStackLock);
	int retVal;
	int errVal;
	WSAData wsaData;

	// 디스커넥트 스택 채우기
	USHORT val = 65534;
	for (int i = 0; i < 65535; i++)
	{
		m_arrDisconnectIndex[i] = val--;
	}

	m_disconnectArrTop = 65534;

	retVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (retVal != 0)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"WSAStartup() 실패 : %d", errVal);
		return FALSE;
	}

	m_sListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
	if (m_sListenSocket == INVALID_SOCKET)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"WSASocket() 실패 : %d", errVal);
		return FALSE;
	}

	m_uiMaxWorkerThreadCount = maxWorkerThreadCount;

	SOCKADDR_IN serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	InetPtonA(AF_INET, openIP, &serverAddr.sin_addr);
	serverAddr.sin_port = htons(port);

	retVal = bind(m_sListenSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"bind() 실패 : %d", errVal);
		return FALSE;
	}

	// 송신 버퍼를 0으로 만들어서 실직적인 I/O를 우리의 버퍼를 이용하도록 만듬
	DWORD sendBufferSize = 0;
	retVal = setsockopt(m_sListenSocket, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufferSize, sizeof(DWORD));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"setsockopt(SO_SNDBUF) 실패 : %d", errVal);
		return FALSE;
	}

	// LINGER option 설정
	LINGER ling;
	ling.l_linger = 0;
	ling.l_onoff = 1;
	retVal = setsockopt(m_sListenSocket, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof(ling));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"setsockopt(SO_LINGER) 실패 : %d", errVal);
		return FALSE;
	}

	// listen
	retVal = listen(m_sListenSocket, SOMAXCONN_HINT(65535));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"listen() 실패 : %d", errVal);
		return FALSE;
	}

	// CP 핸들 생성
	m_hIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, m_uiMaxWorkerThreadCount);
	if (m_hIOCPHandle == NULL)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"CreateIoCompletionPort(생성) 실패 : %d", errVal);
		return FALSE;
	}

	CreateIoCompletionPort((HANDLE)m_sListenSocket, m_hIOCPHandle, (ULONG_PTR)0, 0);

	DWORD dwBytes;
	retVal = WSAIoctl(m_sListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidAcceptEx, sizeof(m_guidAcceptEx), &m_lpfnAcceptEx, sizeof(m_lpfnAcceptEx), &dwBytes, NULL, NULL);
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"WSAIoctl(lpfnAcceptEx) 실패 : %d", errVal);
		return FALSE;
	}

	retVal = WSAIoctl(m_sListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidGetAcceptExSockaddrs, sizeof(m_guidGetAcceptExSockaddrs), &m_lpfnGetAcceptExSockaddrs, sizeof(m_lpfnGetAcceptExSockaddrs), &dwBytes, NULL, NULL);
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"WSAIoctl(lpfnGetAcceptExSockaddrs) 실패 : %d", errVal);
		return FALSE;
	}

	// 500개 AcceptEx 예약
	FristPostAcceptEx();

	// CreateWorkerThread
	for (int i = 1; i <= createWorkerThreadCount; i++)
	{
		HANDLE hWorkerThread = (HANDLE)_beginthreadex(nullptr, 0, WorkerThreadFunc, nullptr, 0, nullptr);
		if (hWorkerThread == 0)
		{
			errVal = GetLastError();
			g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"WorkerThread[%d] running fail.. : %d", i, errVal);
			return FALSE;
		}

		m_arrWorkerThreads.push_back(hWorkerThread);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"[SYSTEM] WorkerThread[%d] running..", i);
	}

	// CreatePostAcceptExThread
	m_hPostAcceptExThread = (HANDLE)_beginthreadex(nullptr, 0, PostAcceptThreadFunc, nullptr, 0, nullptr);
	if (m_hPostAcceptExThread == 0)
	{
		errVal = GetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"PostAcceptExThread running fail.. : %d", errVal);
		return FALSE;
	}

	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"[SYSTEM] PostAcceptExThread running..");

	return TRUE;
}

void CLanServer::SendPacket(const UINT64 sessionID, CSerializableBuffer *sBuffer)
{
	CSession *pSession = m_arrPSessions[GetIndex(sessionID)];
	if (pSession->m_uiSessionID != sessionID)
		__debugbreak();

	USHORT header = sBuffer->GetDataSize();
	if (header != sizeof(__int64))
		__debugbreak();
	sBuffer->EnqueueHeader((char *)&header, sizeof(USHORT));
	pSession->SendPacket(sBuffer);
	pSession->PostSend(1);
}

BOOL CLanServer::Disconnect(CSession *pSession)
{
	InterlockedExchange((LONG *)&pSession->m_bIsValid, FALSE);
	return TRUE;
}

BOOL CLanServer::ReleaseSession(CSession *pSession)
{
	if (pSession->m_bIsValid)
		Disconnect(pSession);

	// ReleaseSession 당시에 남아있는 send 링버퍼를 확인
	// * 남아있는 경우가 확인됨
	// * 남은 직렬화 버퍼를 할당 해제하고 세션 삭제
	int useCount = (pSession->m_SendBuffer.GetUseSize() / sizeof(VOID *));
	for (int i = 0; i < useCount; i++)
	{
		CSerializableBuffer *delBuffer;
		pSession->m_SendBuffer.Peek((char *)&delBuffer, sizeof(VOID *), sizeof(VOID *) * i);
		CSerializableBuffer::Free(delBuffer);
	}
	pSession->m_SendBuffer.MoveFront(useCount * sizeof(VOID *));

	USHORT index = GetIndex(pSession->m_uiSessionID);
	m_arrPSessions[index] = nullptr;
	closesocket(pSession->m_sSessionSocket);
	// delete pSession;
	CSession::Free(pSession);
	InterlockedDecrement(&m_iSessionCount);

	AcquireSRWLockExclusive(&m_disconnectStackLock);
	m_arrDisconnectIndex[++m_disconnectArrTop] = index;
	ReleaseSRWLockExclusive(&m_disconnectStackLock);

	return TRUE;
}

void CLanServer::FristPostAcceptEx()
{
	// 처음에 AcceptEx를 걸어두고 시작
	for (int i = 0; i < ACCEPTEX_COUNT; i++)
	{
		PostAcceptEx(i);
	}

	m_freeAcceptExArrTop = -1;
}

BOOL CLanServer::PostAcceptEx(INT index)
{
	int retVal;
	int errVal;

	CSession *newAcceptEx = CSession::Alloc();
	m_arrAcceptExSessions[index] = newAcceptEx;

	newAcceptEx->m_sSessionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (newAcceptEx->m_sSessionSocket == INVALID_SOCKET)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"PostAcceptEx socket() 실패 : %d", errVal);
		return FALSE;
	}

	InterlockedIncrement(&newAcceptEx->m_iIOCount);
	ZeroMemory(&newAcceptEx->m_AcceptExOverlapped, sizeof(OVERLAPPED));
	newAcceptEx->m_AcceptExOverlapped.m_Index = index;

	retVal = m_lpfnAcceptEx(m_sListenSocket, newAcceptEx->m_sSessionSocket
		, newAcceptEx->m_AcceptBuffer, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, NULL, (LPWSAOVERLAPPED)&newAcceptEx->m_AcceptExOverlapped);
	if (retVal == FALSE)
	{
		errVal = WSAGetLastError();
		if (errVal != WSA_IO_PENDING)
		{
			if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET)
				g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"AcceptEx() Error : %d", errVal);

			// 사실 여기선 0이 될 일이 없음
			// 반환값을 사용안해도 됨
			if (InterlockedDecrement(&newAcceptEx->m_iIOCount) == 0)
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

BOOL CLanServer::AcceptExCompleted(CSession *pSession)
{
	int retVal;
	int errVal;
	retVal = setsockopt(pSession->m_sSessionSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
		(char *)&m_sListenSocket, sizeof(SOCKET));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"setsockopt(SO_UPDATE_ACCEPT_CONTEXT) 실패 : %d", errVal);
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

	AcquireSRWLockExclusive(&m_disconnectStackLock);
	USHORT index = m_arrDisconnectIndex[m_disconnectArrTop--];
	ReleaseSRWLockExclusive(&m_disconnectStackLock);

	UINT64 combineId = CLanServer::CombineIndex(index, ++m_iCurrentID);

	pSession->Init(combineId);

	InterlockedIncrement(&m_iSessionCount);
	InterlockedIncrement(&g_monitor.m_lAcceptTPS);
	m_arrPSessions[index] = pSession;

	return TRUE;
}

int CLanServer::WorkerThread()
{
	int retVal;
	DWORD flag = 0;
	while (m_bIsWorkerRun)
	{
		DWORD dwTransferred = 0;
		CSession *pSession = nullptr;
		OverlappedEx *lpOverlapped = nullptr;

		retVal = GetQueuedCompletionStatus(m_hIOCPHandle, &dwTransferred
										, (PULONG_PTR)&pSession, (LPOVERLAPPED *)&lpOverlapped
										, INFINITE);

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
		else if (dwTransferred == 0 && lpOverlapped->m_Operation != IOOperation::ACCEPTEX)
		{
			Disconnect(pSession);
		}
		else
		{
			switch (lpOverlapped->m_Operation)
			{
			case IOOperation::ACCEPTEX:
			{
				// Accept가 성공한 세션 포인터를 얻어옴
				INT index = lpOverlapped->m_Index;
				pSession = m_arrAcceptExSessions[index];

				// 이거 실패하면 연결 끊음
				// * 실패 가능한 상황 - setsockopt, OnConnectionRequest
				// * ioCount 무조건 1일 것임
				// * 바로 끊어도 괜춘
				// * 다른 I/O 요청을 안걸고 끝내기 때문에 아래의 ioCount 0이 됨으로 연결 끊김을 유도
				if (!AcceptExCompleted(pSession))
				{
					// 실패한 인덱스에 대한 예약은 다시 걸어줌
					PostAcceptAPCEnqueue(index);
					break;
				}

				// OnAccept 처리는 여기서
				OnAccept(pSession->m_uiSessionID);

				// 해당 세션에 대해 Recv 예약
				pSession->PostRecv();

				// 해제된 인덱스에 다시 AcceptEx를 검
				PostAcceptAPCEnqueue(index);
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
				pSession->PostSend(0);
			}
				break;
			}
		}

		LONG back = InterlockedDecrement(&pSession->m_iIOCount);
		if (back == 0)
		{
			ReleaseSession(pSession);
		}
	}

	return 0;
}

int CLanServer::PostAcceptThread()
{
	while (m_bIsPostAcceptExRun)
	{
		// Alertable Wait 상태로 전환
		SleepEx(INFINITE, TRUE);
	}

	return 0;
}

void CLanServer::PostAcceptAPCEnqueue(INT index)
{
	int retVal;
	int errVal;

	AcquireSRWLockExclusive(&m_freeAcceptExStackLock);
	m_arrFreeAcceptExIndex[++m_freeAcceptExArrTop] = index;
	ReleaseSRWLockExclusive(&m_freeAcceptExStackLock);

	retVal = QueueUserAPC(PostAcceptAPCFunc, m_hPostAcceptExThread, (ULONG_PTR)this);
	if (retVal == FALSE)
	{
		errVal = GetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"QueueUserAPC() 실패 : %d", errVal);
	}
}

void CLanServer::PostAcceptAPCFunc(ULONG_PTR lpParam)
{
	CLanServer *pServer = (CLanServer *)lpParam;

	// 스택이 빌때까지 반복
	// 어차피 스택의 Dequeue는 한 스레드에서만 일어남
	while (pServer->m_freeAcceptExArrTop >= 0)
	{
		// Enqueue와 Dequeue의 제어를 위해
		AcquireSRWLockExclusive(&pServer->m_freeAcceptExStackLock);
		USHORT index = pServer->m_arrFreeAcceptExIndex[pServer->m_freeAcceptExArrTop--];
		ReleaseSRWLockExclusive(&pServer->m_freeAcceptExStackLock);

		pServer->PostAcceptEx(index);
	}

	return;
}
