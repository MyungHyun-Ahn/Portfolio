#include "pch.h"
#include "ServerConfig.h"
#include "CLanServer.h"
#include "CSession.h"
#include "CSocketPool.h"

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

	// ��Ŀ��Ʈ ���� ä���
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

	m_hDummyIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, m_uiMaxWorkerThreadCount);
	if (m_hDummyIOCPHandle == NULL)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"CreateIoCompletionPort(����) ���� : %d", errVal);
		return FALSE;
	}

	CreateIoCompletionPort((HANDLE)m_sListenSocket, m_hIOCPHandle, (ULONG_PTR)0, 0);

	DWORD dwBytes;
	retVal = WSAIoctl(m_sListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidAcceptEx, sizeof(m_guidAcceptEx), &m_lpfnAcceptEx, sizeof(m_lpfnAcceptEx), &dwBytes, NULL, NULL);
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"WSAIoctl(lpfnAcceptEx) ���� : %d", errVal);
		return FALSE;
	}

	retVal = WSAIoctl(m_sListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidGetAcceptExSockaddrs, sizeof(m_guidGetAcceptExSockaddrs), &m_lpfnGetAcceptExSockaddrs, sizeof(m_lpfnGetAcceptExSockaddrs), &dwBytes, NULL, NULL);
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"WSAIoctl(lpfnGetAcceptExSockaddrs) ���� : %d", errVal);
		return FALSE;
	}

	retVal = WSAIoctl(m_sListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidDisconnectEx, sizeof(m_guidDisconnectEx), &m_lpfnDisconnectEx, sizeof(m_lpfnDisconnectEx), &dwBytes, NULL, NULL);
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"WSAIoctl(lpfnDisconnectEx) ���� : %d", errVal);
		return FALSE;
	}

	CSocketPool::Init(500);

	// 500�� AcceptEx ����
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
	int retVal;
	int errVal;

	if (pSession->m_bIsValid)
		Disconnect(pSession);

	if (pSession->m_iIOCount != 0)
		__debugbreak();

	// ReleaseSession ��ÿ� �����ִ� send �����۸� Ȯ��
	// * �����ִ� ��찡 Ȯ�ε�
	// * ���� ����ȭ ���۸� �Ҵ� �����ϰ� ���� ����
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

	PostDisconnectEx(pSession);

	return TRUE;
}

void CLanServer::FristPostAcceptEx()
{
	// ó���� AcceptEx�� �ɾ�ΰ� ����
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

	newAcceptEx->m_sSessionSocket = CSocketPool::Alloc();

	InterlockedIncrement(&newAcceptEx->m_iIOCount);
	ZeroMemory(&newAcceptEx->m_AcceptExOverlapped, sizeof(OVERLAPPED));
	newAcceptEx->m_AcceptExOverlapped.m_Index = index;
	newAcceptEx->m_AcceptExOverlapped.m_Ptr = (ULONG_PTR)newAcceptEx;

	retVal = m_lpfnAcceptEx(m_sListenSocket, newAcceptEx->m_sSessionSocket
		, newAcceptEx->m_AcceptBuffer, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, NULL, (LPWSAOVERLAPPED)&newAcceptEx->m_AcceptExOverlapped);
	if (retVal == FALSE)
	{
		errVal = WSAGetLastError();
		if (errVal != WSA_IO_PENDING)
		{
			if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET)
				g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"AcceptEx() Error : %d", errVal);

			// ��� ���⼱ 0�� �� ���� ����
			// ��ȯ���� �����ص� ��
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
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"setsockopt(SO_UPDATE_ACCEPT_CONTEXT) ���� : %d", errVal);
		return FALSE;
	}

	// ������ ���Ͽ� ���� IOCP ���
	HANDLE han = CreateIoCompletionPort((HANDLE)pSession->m_sSessionSocket, m_hIOCPHandle, (ULONG_PTR)pSession, 0);
	if (han == NULL)
		__debugbreak();

	SOCKADDR_IN *localAddr = nullptr;
	INT localAddrLen;

	SOCKADDR_IN *remoteAddr = nullptr;
	INT remoteAddrLen;
	m_lpfnGetAcceptExSockaddrs(pSession->m_AcceptBuffer, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, (SOCKADDR **)&localAddr, &localAddrLen, (SOCKADDR **)&remoteAddr, &remoteAddrLen);
	
	InetNtop(AF_INET, &remoteAddr->sin_addr, pSession->m_ClientAddrBuffer, 16);

	pSession->m_ClientPort = remoteAddr->sin_port;

	// TODO - ������ ��� ���
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

BOOL CLanServer::PostDisconnectEx(CSession *pSession)
{
	int retVal;
	int errVal;

	ZeroMemory(&pSession->m_DisconnectExOverlapped, sizeof(OVERLAPPED));
	pSession->m_DisconnectExOverlapped.m_Ptr = (ULONG_PTR)pSession;

	retVal = m_lpfnDisconnectEx(pSession->m_sSessionSocket, (LPOVERLAPPED)&pSession->m_DisconnectExOverlapped, TF_REUSE_SOCKET, 0);
	if (retVal == FALSE)
	{
		errVal = WSAGetLastError();
		if (errVal != WSA_IO_PENDING)
		{
			// PENDING �ƴ� ����
			g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"DisconnectEx() Error : %d", errVal);
			return FALSE;
		}

		// ����
	}

	return TRUE;
}

BOOL CLanServer::DisconnectExCompleted(CSession *pSession)
{
	USHORT index = GetIndex(pSession->m_uiSessionID);
	CSocketPool::Free(pSession->m_sSessionSocket);
	CSession::Free(pSession);
	InterlockedDecrement(&m_iSessionCount);

	AcquireSRWLockExclusive(&m_disconnectStackLock);
	m_arrDisconnectIndex[++m_disconnectArrTop] = index;
	ReleaseSRWLockExclusive(&m_disconnectStackLock);

	return TRUE;
}

int CLanServer::WorkerThread()
{
	int retVal;
	while (m_bIsWorkerRun)
	{
		DWORD flag = 0;
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
				// ���� ���� ��ƾ
				PostQueuedCompletionStatus(m_hIOCPHandle, 0, 0, 0);
				break;
			}
		}
		// ���� ���� ����
		else if (dwTransferred == 0 && lpOverlapped->m_Operation != IOOperation::ACCEPTEX && lpOverlapped->m_Operation != IOOperation::DISCONNECTEX)
		{
			pSession = (CSession *)lpOverlapped->m_Ptr;

			Disconnect(pSession);
		}
		else
		{
			// pSession = (CSession *)lpOverlapped->m_Ptr;

			switch (lpOverlapped->m_Operation)
			{
			case IOOperation::ACCEPTEX:
			{
				// Accept�� ������ ���� �����͸� ����
				INT index = lpOverlapped->m_Index;
				pSession = m_arrAcceptExSessions[index];

				// �̰� �����ϸ� ���� ����
				// * ���� ������ ��Ȳ - setsockopt, OnConnectionRequest
				// * ioCount ������ 1�� ����
				// * �ٷ� ��� ����
				// * �ٸ� I/O ��û�� �Ȱɰ� ������ ������ �Ʒ��� ioCount 0�� ������ ���� ������ ����
				if (!AcceptExCompleted(pSession))
				{
					// ������ �ε����� ���� ������ �ٽ� �ɾ���
					PostAcceptAPCEnqueue(index);
					break;
				}

				// OnAccept ó���� ���⼭
				OnAccept(pSession->m_uiSessionID);

				// �ش� ���ǿ� ���� Recv ����
				pSession->PostRecv();

				// ������ �ε����� �ٽ� AcceptEx�� ��
				PostAcceptAPCEnqueue(index);
			}
				break;
			case IOOperation::DISCONNECTEX:
			{
				// ������ ������ ������ ó��
				flag = 1;
				DisconnectExCompleted(pSession);
			}
				break;
			case IOOperation::RECV:
			{
				if ((ULONG_PTR)pSession != lpOverlapped->m_Ptr)
					__debugbreak();

				pSession->RecvCompleted(dwTransferred);
				pSession->PostRecv();
			}
				break;
			case IOOperation::SEND:
			{
				if ((ULONG_PTR)pSession != lpOverlapped->m_Ptr)
					__debugbreak();

				pSession->SendCompleted(dwTransferred);
				pSession->PostSend(0);
			}
				break;
			}
		}

		if (flag)
			continue;

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
		// Alertable Wait ���·� ��ȯ
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
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"QueueUserAPC() ���� : %d", errVal);
	}
}

void CLanServer::PostAcceptAPCFunc(ULONG_PTR lpParam)
{
	CLanServer *pServer = (CLanServer *)lpParam;

	// ������ �������� �ݺ�
	// ������ ������ Dequeue�� �� �����忡���� �Ͼ
	while (pServer->m_freeAcceptExArrTop >= 0)
	{
		// Enqueue�� Dequeue�� ��� ����
		AcquireSRWLockExclusive(&pServer->m_freeAcceptExStackLock);
		USHORT index = pServer->m_arrFreeAcceptExIndex[pServer->m_freeAcceptExArrTop--];
		ReleaseSRWLockExclusive(&pServer->m_freeAcceptExStackLock);

		pServer->PostAcceptEx(index);
	}

	return;
}
