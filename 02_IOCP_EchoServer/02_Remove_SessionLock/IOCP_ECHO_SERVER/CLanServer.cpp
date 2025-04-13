#include "pch.h"
#include "ServerConfig.h"
#include "CLanServer.h"
#include "CSession.h"

CLanServer *g_Server;
std::unordered_map<UINT64, CSession *> g_mapSessions;
CRITICAL_SECTION g_SessionMapLock;

LONG createCount = 0;
LONG deleteCount = 0;

unsigned int AcceptorThreadFunc(LPVOID lpParam)
{
	return g_Server->AccepterThread();
}

unsigned int WorkerThreadFunc(LPVOID lpParam)
{
	return g_Server->WorkerThread();
}

BOOL CLanServer::Start(const CHAR *openIP, const USHORT port, USHORT createWorkerThreadCount, USHORT maxWorkerThreadCount, INT maxSessionCount)
{
	InitializeCriticalSection(&g_SessionMapLock);
	int retVal;
	int errVal;
	WSAData wsaData;

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

	// CreateAcceptThread
	m_hAcceptorThread = (HANDLE)_beginthreadex(nullptr, 0, AcceptorThreadFunc, nullptr, 0, nullptr);
	if (m_hAcceptorThread == 0)
	{
		errVal = GetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"AccepterThread running fail.. : %d", errVal);
		return FALSE;

	}

	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"[SYSTEM] AccepterThread running..");

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

	return TRUE;
}

void CLanServer::SendPacket(const UINT64 sessionID, CSerializableBuffer *sBuffer)
{
	EnterCriticalSection(&g_SessionMapLock);
	CSession *pSession = g_mapSessions[sessionID];
	LeaveCriticalSection(&g_SessionMapLock);

	USHORT header = sBuffer->GetDataSize();
	if (header != sizeof(__int64))
		__debugbreak();
	sBuffer->EnqueueHeader((char *)&header, sizeof(USHORT));
	pSession->SendPacket(sBuffer);
	if (!pSession->PostSend(1))
		ReleaseSession(pSession);
}

BOOL CLanServer::Disconnect(CSession *pSession)
{
	InterlockedExchange((LONG *)&pSession->m_bIsValid, FALSE);
	return TRUE;
}

BOOL CLanServer::ReleaseSession(CSession *pSession)
{
	EnterCriticalSection(&g_SessionMapLock);
	g_mapSessions.erase(pSession->m_uiSessionID);
	LeaveCriticalSection(&g_SessionMapLock);

	if (pSession->m_bIsValid)
		Disconnect(pSession);

	closesocket(pSession->m_sSessionSocket);
	delete pSession;

	InterlockedIncrement(&deleteCount);

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
				PostQueuedCompletionStatus(m_hIOCPHandle, 0, 0, 0);
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
		else if (dwTransferred == 0)
		{
			Disconnect(pSession);
		}
		else
		{
			if (lpOverlapped->m_Operation == IOOperation::RECV)
			{
				pSession->RecvCompleted(dwTransferred);

				// 여기까지 왔는데 Send 버퍼에 남은게 있음
				// 그리고 SendCompleted 후의 SendPost가 씹혔다면 Send 봉인됨
				// ioCount가 1이라면 SendPost가 씹힌 것
				// if (pSession->m_SendBuffer.GetUseSize() && pSession->m_iIOCount == 1);
				// {
				// 	__debugbreak();
				// }

				// 여기에서 SendFlag가 FALSE인 경우
				// if (!pSession->m_iSendFlag)
				// 	__debugbreak();

				pSession->PostRecv();
			}
			else if (lpOverlapped->m_Operation == IOOperation::SEND)
			{
				pSession->SendCompleted(dwTransferred);
				pSession->PostSend(0);

			}
		}

		LONG back = InterlockedDecrement(&pSession->m_iIOCount);
		if (back == 0)
			ReleaseSession(pSession);
	}

	return 0;
}

int CLanServer::AccepterThread()
{
	int errVal;
	while (m_bIsAcceptorRun)
	{
		SOCKADDR_IN clientAddr;
		int addrLen = sizeof(clientAddr);

		DWORD flag = 0;

		SOCKET clientSocket = accept(m_sListenSocket, (SOCKADDR *)&clientAddr, &addrLen);
		if (clientSocket == INVALID_SOCKET)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"accept() 실패 : %d", errVal);
			return FALSE;
		}

		WCHAR clientAddrBuf[16] = { 0, };
		InetNtop(AF_INET, &clientAddr.sin_addr, clientAddrBuf, 16);

		// 접속 허용 IP 확인
		if (!OnConnectionRequest(clientAddrBuf, ntohs(clientAddr.sin_port)))
			continue;

		CSession *pSession = new CSession(clientSocket, ++m_iCurrentID);

		InterlockedIncrement(&createCount);

		// IOCP에 세션 소켓 등록
		CreateIoCompletionPort((HANDLE)clientSocket, m_hIOCPHandle, (ULONG_PTR)pSession, 0);
		// g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"[SYSTEM] ip : %s , port : %d, sessionID : %lld\n", clientAddrBuf, ntohs(clientAddr.sin_port), pSession->m_uiSessionID);

		EnterCriticalSection(&g_SessionMapLock);
		g_mapSessions.insert(std::make_pair(pSession->m_uiSessionID, pSession));
		LeaveCriticalSection(&g_SessionMapLock);

		OnAccept(pSession->m_uiSessionID);
		pSession->PostRecv();
	}

	return 0;
}
