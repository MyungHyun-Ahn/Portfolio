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
	InitializeSRWLock(&m_disconnectStackLock);
	int retVal;
	int errVal;
	WSAData wsaData;

	// ��Ŀ��Ʈ ���� ä���
	for (int i = 65534; i >= 0; i--)
	{
		m_stackDisconnectIndex.push_back(i);
	}

	retVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (retVal != 0)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"WSAStartup() ���� : %d", errVal);
		return FALSE;
	}

	m_sListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
	if (m_sListenSocket == INVALID_SOCKET)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"WSASocket() ���� : %d", errVal);
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
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"bind() ���� : %d", errVal);
		return FALSE;
	}

	// �۽� ���۸� 0���� ���� �������� I/O�� �츮�� ���۸� �̿��ϵ��� ����
	DWORD sendBufferSize = 0;
	retVal = setsockopt(m_sListenSocket, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufferSize, sizeof(DWORD));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"setsockopt(SO_SNDBUF) ���� : %d", errVal);
		return FALSE;
	}

	// LINGER option ����
	LINGER ling;
	ling.l_linger = 0;
	ling.l_onoff = 1;
	retVal = setsockopt(m_sListenSocket, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof(ling));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"setsockopt(SO_LINGER) ���� : %d", errVal);
		return FALSE;
	}

	// listen
	retVal = listen(m_sListenSocket, SOMAXCONN_HINT(65535));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"listen() ���� : %d", errVal);
		return FALSE;
	}

	// CP �ڵ� ����
	m_hIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, m_uiMaxWorkerThreadCount);
	if (m_hIOCPHandle == NULL)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"CreateIoCompletionPort(����) ���� : %d", errVal);
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

#ifdef SESSION_LOCK
	EnterCriticalSection(&pSession->m_Lock);
#endif
	LeaveCriticalSection(&g_SessionMapLock);

	USHORT header = sBuffer->GetDataSize();
	if (header != sizeof(__int64))
		__debugbreak();
	sBuffer->EnqueueHeader((char *)&header, sizeof(USHORT));
	pSession->SendPacket(sBuffer);
	if (!pSession->PostSend(1))
		ReleaseSession(pSession);
#ifdef SESSION_LOCK
	LeaveCriticalSection(&pSession->m_Lock);
#endif
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

#ifdef SESSION_LOCK
	EnterCriticalSection(&pSession->m_Lock);
#endif
	LeaveCriticalSection(&g_SessionMapLock);
	if (pSession->m_bIsValid)
		Disconnect(pSession);

	closesocket(pSession->m_sSessionSocket);
#ifdef SESSION_LOCK
	LeaveCriticalSection(&pSession->m_Lock);
#endif
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
				// ���� ���� ��ƾ
				PostQueuedCompletionStatus(m_hIOCPHandle, 0, 0, 0);
				break;
			}
		}
		// ���� ���� ����
		else if (dwTransferred == 0)
		{
			Disconnect(pSession);
		}
		else
		{
			if (lpOverlapped->m_Operation == IOOperation::RECV)
			{
				pSession->RecvCompleted(dwTransferred);

				// ������� �Դµ� Send ���ۿ� ������ ����
				// �׸��� SendCompleted ���� SendPost�� �����ٸ� Send ���ε�
				// ioCount�� 1�̶�� SendPost�� ���� ��
				// if (pSession->m_SendBuffer.GetUseSize() && pSession->m_iIOCount == 1);
				// {
				// 	__debugbreak();
				// }

				pSession->PostSend(2);
				pSession->PostRecv();
			}
			else if (lpOverlapped->m_Operation == IOOperation::SEND)
			{
#ifdef SESSION_LOCK
				EnterCriticalSection(&pSession->m_Lock);
#endif
				pSession->SendCompleted(dwTransferred);
				pSession->PostSend(0);

#ifdef SESSION_LOCK
				LeaveCriticalSection(&pSession->m_Lock);
#endif
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
			g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"accept() ���� : %d", errVal);
			return FALSE;
		}

		WCHAR clientAddrBuf[16] = { 0, };
		InetNtop(AF_INET, &clientAddr.sin_addr, clientAddrBuf, 16);

		// ���� ��� IP Ȯ��
		if (!OnConnectionRequest(clientAddrBuf, ntohs(clientAddr.sin_port)))
			continue;

		CSession *pSession = new CSession(clientSocket, ++m_iCurrentID);

		InterlockedIncrement(&createCount);

		// IOCP�� ���� ���� ���
		CreateIoCompletionPort((HANDLE)clientSocket, m_hIOCPHandle, (ULONG_PTR)pSession, 0);
		// g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"[SYSTEM] ip : %s , port : %d, sessionID : %lld\n", clientAddrBuf, ntohs(clientAddr.sin_port), pSession->m_uiSessionID);

		// AcquireSRWLockExclusive(&m_disconnectStackLock);
		// USHORT index = m_stackDisconnectIndex.back();
		// m_stackDisconnectIndex.pop_back();
		// ReleaseSRWLockExclusive(&m_disconnectStackLock);
		// m_arrPSessions[index] = pSession;

		EnterCriticalSection(&g_SessionMapLock);
		g_mapSessions.insert(std::make_pair(pSession->m_uiSessionID, pSession));
		LeaveCriticalSection(&g_SessionMapLock);

		OnAccept(pSession->m_uiSessionID);
		pSession->PostRecv();
	}

	return 0;
}
