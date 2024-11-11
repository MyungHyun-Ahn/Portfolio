#include "pch.h"
#include "ServerConfig.h"
#include "CLanServer.h"
#include "CSession.h"

CLanServer *g_Server;

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
	InitializeSRWLock(&m_disconnectStackLock);
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

		if (m_iSessionCount > 65535)
		{
			closesocket(clientSocket);
			continue;
		}

		WCHAR clientAddrBuf[16] = { 0, };
		InetNtop(AF_INET, &clientAddr.sin_addr, clientAddrBuf, 16);

		// 접속 허용 IP 확인
		if (!OnConnectionRequest(clientAddrBuf, ntohs(clientAddr.sin_port)))
			continue;

		AcquireSRWLockExclusive(&m_disconnectStackLock);
		USHORT index = m_arrDisconnectIndex[m_disconnectArrTop--];
		ReleaseSRWLockExclusive(&m_disconnectStackLock);

		UINT64 combineId = CombineIndex(index, ++m_iCurrentID);

		// CSession *pSession = new CSession(clientSocket, combineId);
		CSession *pSession = CSession::Alloc();
		pSession->Init(clientSocket, combineId);

		InterlockedIncrement(&m_iSessionCount);
		InterlockedIncrement(&createCount);

		// IOCP에 세션 소켓 등록
		CreateIoCompletionPort((HANDLE)clientSocket, m_hIOCPHandle, (ULONG_PTR)pSession, 0);
		// g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"[SYSTEM] ip : %s , port : %d, sessionID : %lld\n", clientAddrBuf, ntohs(clientAddr.sin_port), pSession->m_uiSessionID);

		m_arrPSessions[index] = pSession;

		InterlockedIncrement(&pSession->m_iIOCount);
		OnAccept(pSession->m_uiSessionID);
		pSession->PostRecv();
		if (InterlockedDecrement(&pSession->m_iIOCount) == 0)
		{
			ReleaseSession(pSession);
		}

		InterlockedIncrement(&g_monitor.m_lAcceptTPS);
	}

	return 0;
}
