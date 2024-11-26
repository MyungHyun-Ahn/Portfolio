#include "pch.h"
#include "DefineNetwork.h"
#include "CSession.h"
#include "CLanClients.h"

CLanClients *g_Clients;

unsigned int WorkerThreadFunc(LPVOID lpParam)
{
	return g_Clients->WorkerThread();
}


unsigned int PostConnectExThreadFunc(LPVOID lpParam)
{
	return g_Clients->PostConnectExThread();
}

bool CLanClients::Start(const CHAR *serverIP, const USHORT port, USHORT createWorkerThreadCount, USHORT maxWorkerThreadCount, INT sessionCount)
{
	int retVal;
	int errVal;
	WSAData wsaData;

	ZeroMemory(&m_ServerAddr, sizeof(m_ServerAddr));
	m_ServerAddr.sin_family = AF_INET;
	inet_pton(AF_INET, serverIP, &m_ServerAddr.sin_addr.s_addr);
	m_ServerAddr.sin_port = htons(port);

	m_szServerIP = serverIP;
	m_usServerPort = port;
	m_usMaxSessionCount = sessionCount;
	m_arrConnectExSessions = new CSession * [m_usMaxSessionCount];

	// ��Ŀ��Ʈ ���� ä���
	USHORT val = 65534;
	for (int i = 0; i < 65535; i++)
	{
		m_stackDisconnectIndex.Push(val--);
	}

	retVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (retVal != 0)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"WSAStartup() ���� : %d", errVal);
		return false;
	}

	// CP �ڵ� ����
	m_hIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, m_uiMaxWorkerThreadCount);
	if (m_hIOCPHandle == NULL)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"CreateIoCompletionPort(����) ���� : %d", errVal);
		return false;
	}

	m_hPostConnectExThread = (HANDLE)_beginthreadex(nullptr, 0, PostConnectExThreadFunc, nullptr, 0, nullptr);
	if (m_hPostConnectExThread == 0)
	{
		errVal = GetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"PostConnectExThread running fail.. : %d", errVal);
		return false;
	}

	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"[SYSTEM] PostConnectExThread running..");

	FristPostConnectEx();

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

	return true;
}

bool CLanClients::SetSockOpt(SOCKET sock)
{
	int retVal;
	int errVal;
	// �۽� ���۸� 0���� ���� �������� I/O�� �츮�� ���۸� �̿��ϵ��� ����
	DWORD sendBufferSize = 0;
	retVal = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufferSize, sizeof(DWORD));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"setsockopt(SO_SNDBUF) ���� : %d", errVal);
		return false;
	}

	// LINGER option ����
	LINGER ling;
	ling.l_linger = 0;
	ling.l_onoff = 1;
	retVal = setsockopt(sock, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof(ling));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"setsockopt(SO_LINGER) ���� : %d", errVal);
		return false;
	}

	return true;
}

void CLanClients::SendPacket(const UINT64 sessionID, CSerializableBuffer *sBuffer, bool sendPost)
{
	USHORT index = GetIndex(sessionID);
	CSession *pSession = m_arrPSessions[index];
	if (pSession->m_uiSessionID != sessionID)
		__debugbreak();

	USHORT header = sBuffer->GetDataSize();
	// if (header != sizeof(__int64))
	// 	__debugbreak();
	sBuffer->EnqueueHeader((char *)&header, sizeof(USHORT));
	pSession->SendPacket(sBuffer);
	if (sendPost)
		pSession->PostSend(0);
}

bool CLanClients::Disconnect(CSession *pSession)
{
	InterlockedExchange((LONG *)&pSession->m_bIsValid, FALSE);
	return true;
}

bool CLanClients::ReleaseSession(CSession *pSession)
{
	if (pSession->m_bIsValid)
		Disconnect(pSession);
	UINT64 sessionId = pSession->m_uiSessionID;
	USHORT index = GetIndex(sessionId);

	bool isConnected = pSession->m_bIsConnected;
	m_arrPSessions[index] = nullptr;
	closesocket(pSession->m_sSessionSocket);
	CSession::Free(pSession);

	if (isConnected)
	{
		InterlockedDecrement(&m_iSessionCount);
		OnReleaseSession(sessionId);
		m_stackDisconnectIndex.Push(index);
	}
	return true;
}

// ó���� �Ҹ��� �ɰ�
void CLanClients::FristPostConnectEx()
{
	for (int i = 0; i < m_usMaxSessionCount; i++)
	{
		PostConnectEx(i);
	}
}

bool CLanClients::PostConnectEx(INT index)
{
	int retVal;
	int errVal;

	CSession *newConnectEx = CSession::Alloc();
	m_arrConnectExSessions[index] = newConnectEx;

	newConnectEx->m_sSessionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (newConnectEx->m_sSessionSocket == INVALID_SOCKET)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"PostConnectEx socket() ���� : %d", errVal);
		return false;
	}
	ZeroMemory(&newConnectEx->m_SockAddr, sizeof(SOCKADDR_IN));
	newConnectEx->m_SockAddr.sin_family = AF_INET;
	retVal = bind(newConnectEx->m_sSessionSocket, (SOCKADDR *)&newConnectEx->m_SockAddr, sizeof(newConnectEx->m_SockAddr));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"PostConnectEx bind() ���� : %d", errVal);
		return false;
	}

	InterlockedIncrement(&newConnectEx->m_iIOCount);
	ZeroMemory(&newConnectEx->m_ConnectExOverlapped, sizeof(OVERLAPPED));
	newConnectEx->m_ConnectExOverlapped.m_Index = index;

	CreateIoCompletionPort((HANDLE)newConnectEx->m_sSessionSocket, m_hIOCPHandle, (ULONG_PTR)newConnectEx, 0);

	GUID guid = WSAID_CONNECTEX;
	LPFN_CONNECTEX ConnectEx = nullptr;
	unsigned long cbBytes;
	WSAIoctl(newConnectEx->m_sSessionSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ConnectEx, sizeof(ConnectEx), &cbBytes, nullptr, nullptr);

	retVal = ConnectEx(newConnectEx->m_sSessionSocket
		, (SOCKADDR *)&m_ServerAddr, sizeof(m_ServerAddr)
		, NULL, NULL, NULL, (LPOVERLAPPED)&newConnectEx->m_ConnectExOverlapped);
	if (retVal == FALSE)
	{
		errVal = WSAGetLastError();
		if (errVal != WSA_IO_PENDING)
		{
			if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET)
				g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"ConnectEx() Error : %d", errVal);

			if (InterlockedDecrement(&newConnectEx->m_iIOCount) == 0)
			{
				return false;
			}
		}
	}

	return true;
}

bool CLanClients::ConnectExCompleted(CSession *pSession)
{
	int retVal;
	int errVal;

	int seconds;
	int bytes = sizeof(seconds);
	retVal = getsockopt(pSession->m_sSessionSocket, SOL_SOCKET, SO_CONNECT_TIME, (char *)&seconds, (PINT)&bytes);
	if (retVal != NO_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"getsockopt(SO_CONNECT_TIME) failed : %d", errVal);
		return false;
	}
	
	if (seconds == 0xFFFFFFFF)
		return false;

	retVal = SetSockOpt(pSession->m_sSessionSocket);
	if (retVal == FALSE)
	{
		return false;
	}

	USHORT index;
	// ���� ���� : FALSE
	if (!m_stackDisconnectIndex.Pop(&index))
	{
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"m_stackDisconnectIndex.Pop(&index) failed");
		return false;
	}

	UINT64 combineId = CLanClients::CombineIndex(index, InterlockedIncrement64(&m_iCurrentID));

	pSession->Init(combineId);
	m_arrPSessions[index] = pSession;


	InterlockedIncrement(&m_iSessionCount);
	InterlockedIncrement(&g_monitor.m_lConnectTPS);
	InterlockedIncrement64(&g_monitor.m_lConnectTotal);


	return true;
}

int CLanClients::WorkerThread()
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
				// ���� ���� ��ƾ
				PostQueuedCompletionStatus(m_hIOCPHandle, 0, 0, 0);
				break;
			}
		}
		// ���� ���� ����
		else if (dwTransferred == 0 && lpOverlapped->m_Operation != IOOperation::CONNECTEX)
		{
			Disconnect(pSession);
		}
		else
		{
			switch (lpOverlapped->m_Operation)
			{
			case IOOperation::CONNECTEX:
			{
				// Accept�� ������ ���� �����͸� ����
				INT index = lpOverlapped->m_Index;
				pSession = m_arrConnectExSessions[index];
				m_arrConnectExSessions[index] = nullptr;
				m_stackFreeAcceptExIndex.Push(index);

				// �̰� �����ϸ� ���� ����
				// * ���� ������ ��Ȳ - setsockopt, OnConnectionRequest
				// * ioCount ������ 1�� ����
				// * �ٷ� ��� ����
				// * �ٸ� I/O ��û�� �Ȱɰ� ������ ������ �Ʒ��� ioCount 0�� ������ ���� ������ ����
				if (!ConnectExCompleted(pSession))
				{
					PostConnectExAPCEnqueue();
					break;
				}

				// DummyClient ��ü ���� ��
				// OnConnect ó���� ���⼭
				OnConnect(pSession->m_uiSessionID);
				pSession->m_bIsConnected = TRUE;

				// �ش� ���ǿ� ���� Recv ����
				pSession->PostRecv();
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
			}
		}

		LONG back = InterlockedDecrement(&pSession->m_iIOCount);
		if (back == 0)
		{
			// �ϳ� �������� �ֱ�
			ReleaseSession(pSession);
			PostConnectExAPCEnqueue();
		}
	}

	return 0;
}

int CLanClients::PostConnectExThread()
{
	while (m_bIsPostConnectExRun)
	{
		// Alertable Wait ���·� ��ȯ
		SleepEx(INFINITE, TRUE);
	}

	return 0;
}

void CLanClients::PostConnectExAPCEnqueue()
{
	int retVal;
	int errVal;

	retVal = QueueUserAPC(PostConnectExAPCFunc, m_hPostConnectExThread, (ULONG_PTR)this);
	if (retVal == FALSE)
	{
		errVal = GetLastError();
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"QueueUserAPC() ���� : %d", errVal);
	}
}

void CLanClients::PostConnectExAPCFunc(ULONG_PTR lpParam)
{
	CLanClients *pServer = (CLanClients *)lpParam;

	// �� 1���� ����
	USHORT index;
	if (pServer->m_stackFreeAcceptExIndex.Pop(&index))
		pServer->PostConnectEx(index);
}
