#include "pch.h"
#include "ServerSetting.h"
#include "CNetServer.h"
#include "CNetSession.h"

CNetServer *g_NetServer = nullptr;

unsigned int NetWorkerThreadFunc(LPVOID lpParam) noexcept
{
	return g_NetServer->WorkerThread();
}

unsigned int NetServerFrameThreadFunc(LPVOID lpParam) noexcept
{
	return g_NetServer->ServerFrameThread();
}

BOOL CNetServer::Start(const CHAR *openIP, const USHORT port, USHORT createWorkerThreadCount, USHORT maxWorkerThreadCount, INT maxSessionCount) noexcept
{
	int retVal;
	int errVal;
	WSAData wsaData;

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
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSAStartup() ���� : %d", errVal);
		return FALSE;
	}

	m_sListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
	if (m_sListenSocket == INVALID_SOCKET)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSASocket() ���� : %d", errVal);
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
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"bind() ���� : %d", errVal);
		return FALSE;
	}

	// �۽� ���۸� 0���� ���� �������� I/O�� �츮�� ���۸� �̿��ϵ��� ����
	if (USE_ZERO_COPY)
	{
		DWORD sendBufferSize = 0;
		retVal = setsockopt(m_sListenSocket, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufferSize, sizeof(DWORD));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"setsockopt(SO_SNDBUF) ���� : %d", errVal);
			return FALSE;
		}
	}

	// LINGER option ����
	LINGER ling;
	ling.l_linger = 0;
	ling.l_onoff = 1;
	retVal = setsockopt(m_sListenSocket, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof(ling));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"setsockopt(SO_LINGER) ���� : %d", errVal);
		return FALSE;
	}

	// listen
	retVal = listen(m_sListenSocket, SOMAXCONN_HINT(65535));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"listen() ���� : %d", errVal);
		return FALSE;
	}

	// CP �ڵ� ����
	m_hIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, m_uiMaxWorkerThreadCount);
	if (m_hIOCPHandle == NULL)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"CreateIoCompletionPort(����) ���� : %d", errVal);
		return FALSE;
	}

	CreateIoCompletionPort((HANDLE)m_sListenSocket, m_hIOCPHandle, (ULONG_PTR)0, 0);

	DWORD dwBytes;
	retVal = WSAIoctl(m_sListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidAcceptEx, sizeof(m_guidAcceptEx), &m_lpfnAcceptEx, sizeof(m_lpfnAcceptEx), &dwBytes, NULL, NULL);
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSAIoctl(lpfnAcceptEx) ���� : %d", errVal);
		return FALSE;
	}

	retVal = WSAIoctl(m_sListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidGetAcceptExSockaddrs, sizeof(m_guidGetAcceptExSockaddrs), &m_lpfnGetAcceptExSockaddrs, sizeof(m_lpfnGetAcceptExSockaddrs), &dwBytes, NULL, NULL);
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSAIoctl(lpfnGetAcceptExSockaddrs) ���� : %d", errVal);
		return FALSE;
	}

	m_hServerFrameThread = (HANDLE)_beginthreadex(nullptr, 0, NetServerFrameThreadFunc, nullptr, 0, nullptr);
	if (m_hServerFrameThread == 0)
	{
		errVal = GetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"ServerFrameThread running fail.. : %d", errVal);
		return FALSE;
	}

	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"[SYSTEM] ServerFrameThread running..");

	// CreateWorkerThread
	for (int i = 1; i <= createWorkerThreadCount; i++)
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

void CNetServer::SendPacket(const UINT64 sessionID, CSerializableBuffer<FALSE> *sBuffer) noexcept
{
	CNetSession *pSession = m_arrPSessions[GetIndex(sessionID)];

	InterlockedIncrement(&pSession->m_iIOCountAndRelease);
	// ReleaseFlag�� �̹� ���� ��Ȳ
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
		header.code = PACKET_KEY; // �ڵ�
		header.len = sBuffer->GetDataSize();
		header.randKey = rand() % 256;
		header.checkSum = CEncryption::CalCheckSum(sBuffer->GetContentBufferPtr(), sBuffer->GetDataSize());
		sBuffer->EnqueueHeader((char *)&header, sizeof(NetHeader));

		// CheckSum ���� ��ȣȭ�ϱ� ����
		CEncryption::Encoding(sBuffer->GetBufferPtr() + 4, sBuffer->GetBufferSize() - 4, header.randKey);
	}

	pSession->SendPacket(sBuffer);
	pSession->PostSend();

	if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
	{
		ReleaseSession(pSession);
	}
}

BOOL CNetServer::Disconnect(const UINT64 sessionID) noexcept
{
	CNetSession *pSession = m_arrPSessions[GetIndex(sessionID)];

	// ReleaseFlag�� �̹� ���� ��Ȳ
	InterlockedIncrement(&pSession->m_iIOCountAndRelease);
	if ((pSession->m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
	{
		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			ReleaseSession(pSession);
		}
		return FALSE;
	}

	if (sessionID != pSession->m_uiSessionID)
	{
		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			ReleaseSession(pSession);
		}
		return FALSE;
	}

	if (InterlockedExchange(&pSession->m_iCacelIoCalled, TRUE) == TRUE)
	{
		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			ReleaseSession(pSession);
		}
		return FALSE;
	}

	// Io ���� ����
	CancelIoEx((HANDLE)pSession->m_sSessionSocket, nullptr);

	if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
	{
		// IOCount == 0 �̸� ���� �õ�
		ReleaseSession(pSession);
	}

	return TRUE;
}

BOOL CNetServer::ReleaseSession(CNetSession *pSession) noexcept
{
	// IoCount üũ�� ReleaseFlag ������ ���ÿ�
	if (InterlockedCompareExchange(&pSession->m_iIOCountAndRelease, CNetSession::RELEASE_FLAG, 0) != 0)
	{
		// ioCount 0�� �ƴϰų� �̹� Release ���� ���ΰ� �ִٸ� return
		return FALSE;
	}


	USHORT index = GetIndex(pSession->m_uiSessionID);
	closesocket(pSession->m_sSessionSocket);
	UINT64 freeSessionId = pSession->m_uiSessionID;
	CNetSession::Free(pSession);
	InterlockedDecrement(&m_iSessionCount);

	ClientLeaveAPCEnqueue(freeSessionId);

	m_stackDisconnectIndex.Push(index);

	return TRUE;
}

void CNetServer::FristPostAcceptEx() noexcept
{
	// ó���� AcceptEx�� �ɾ�ΰ� ����
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
	// newAcceptEx->m_iIOCountAndRelease = 0;
	m_arrAcceptExSessions[index] = newAcceptEx;

	newAcceptEx->m_sSessionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (newAcceptEx->m_sSessionSocket == INVALID_SOCKET)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"PostAcceptEx socket() ���� : %d", errVal);
		return FALSE;
	}

	// InterlockedIncrement(&newAcceptEx->m_iIOCountAndRelease);
	ZeroMemory(newAcceptEx->m_pMyOverlappedStartAddr, sizeof(OVERLAPPED));
	g_OverlappedAlloc.m_infos[g_OverlappedAlloc.CalIndex((ULONG_PTR)newAcceptEx->m_pMyOverlappedStartAddr)].index = index;

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
	int retVal;
	int errVal;
	retVal = setsockopt(pSession->m_sSessionSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
		(char *)&m_sListenSocket, sizeof(SOCKET));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"setsockopt(SO_UPDATE_ACCEPT_CONTEXT) ���� : %d", errVal);
		return FALSE;
	}

	if (pSession == nullptr)
		__debugbreak();

	// ������ ���Ͽ� ���� IOCP ���
	CreateIoCompletionPort((HANDLE)pSession->m_sSessionSocket, m_hIOCPHandle, (ULONG_PTR)pSession, 0);

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


	USHORT index;
	// ���� ���� : FALSE
	if (!m_stackDisconnectIndex.Pop(&index))
	{
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"m_stackDisconnectIndex.Pop(&index) failed");
		return FALSE;
	}

	UINT64 combineId = CNetServer::CombineIndex(index, InterlockedIncrement64(&m_iCurrentID));

	pSession->Init(combineId);

	InterlockedIncrement(&m_iSessionCount);
	InterlockedIncrement(&g_monitor.m_lAcceptTPS);
	InterlockedIncrement64(&g_monitor.m_lAcceptTotal);
	m_arrPSessions[index] = pSession;

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

		
		OverlappedInfo *overlappedInfo = nullptr;
		if (lpOverlapped != nullptr)
		{
			overlappedInfo = &g_OverlappedAlloc.m_infos[g_OverlappedAlloc.CalIndex((ULONG_PTR)lpOverlapped)];
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
				// ���� ���� ��ƾ
				PostQueuedCompletionStatus(m_hIOCPHandle, 0, 0, 0);
				break;
			}
		}
		// ���� ���� ����
		else if (dwTransferred == 0 && overlappedInfo->operation != IOOperation::ACCEPTEX)
		{
			Disconnect(pSession->m_uiSessionID);
		}
		else
		{
			switch (overlappedInfo->operation)
			{
			case IOOperation::ACCEPTEX:
			{
				// Accept�� ������ ���� �����͸� ����
				INT index = overlappedInfo->index;
				pSession = m_arrAcceptExSessions[index];

				// �̰� �����ϸ� ���� ����
				// * ���� ������ ��Ȳ - setsockopt, OnConnectionRequest
				// * ioCount ������ 1�� ����
				// * �ٷ� ��� ����
				// * �ٸ� I/O ��û�� �Ȱɰ� ������ ������ �Ʒ��� ioCount 0�� ������ ���� ������ ����
				if (!AcceptExCompleted(pSession))
				{
					// ������ �ε����� ���� ������ �ٽ� �ɾ���
					PostAcceptEx(index);
					break;
				}

				InterlockedIncrement(&pSession->m_iIOCountAndRelease);

				// OnAccept ó���� ���⼭
				AcceptAPCEnqueue(pSession->m_uiSessionID);
				// �ش� ���ǿ� ���� Recv ����
				pSession->PostRecv();

				if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
					ReleaseSession(pSession);

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
			}
		}

		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			ReleaseSession(pSession);
		}
	}

	return 0;
}

int CNetServer::ServerFrameThread() noexcept
{
	// IOCP�� ���༺ ������ �ޱ� ���� GQCS
	DWORD dwTransferred = 0;
	CNetSession *pSession = nullptr;
	OVERLAPPED *lpOverlapped = nullptr;

	GetQueuedCompletionStatus(m_hIOCPHandle, &dwTransferred
		, (PULONG_PTR)&pSession, (LPOVERLAPPED *)&lpOverlapped
		, 0);

	// ����� ����ŭ AcceptEx ����
	FristPostAcceptEx();

	while (m_bIsServerFrameRun)
	{
		// Update
		// -> ���⼭ Update ���� ����
		// -> ��ȯ���� ������ ��� ���� 
		DWORD sleepTime = OnUpdate();

		// Heartbeat
		// -> OnHeartbeat�� Disconnect ����
		// OnHeartBeat();

		// Alertable Wait ���·� ��ȯ
		// + ������ ����
		SleepEx(sleepTime, TRUE);
	}

	return 0;
}

void CNetServer::AcceptAPCEnqueue(UINT64 sessionId) noexcept
{
	int retVal;
	int errVal;

	// m_stackFreeAcceptExIndex.Push(index);

	retVal = QueueUserAPC(AcceptAPCFunc, m_hServerFrameThread, (ULONG_PTR)sessionId);
	if (retVal == FALSE)
	{
		errVal = GetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"QueueUserAPC() ���� : %d", errVal);
	}
}

// lpParam���� SessionId�� �Ѿ��
void CNetServer::AcceptAPCFunc(ULONG_PTR lpParam) noexcept
{
	// OnAccept ȣ��
	if (lpParam != 0)
		g_NetServer->OnAccept(lpParam);

	// AcceptEx �۾�
	// USHORT index;
	// while (g_NetServer->m_stackFreeAcceptExIndex.Pop(&index))
	// {
	// 	g_NetServer->PostAcceptEx(index);
	// }
}

void CNetServer::ClientLeaveAPCEnqueue(UINT64 sessionId) noexcept
{
	int retVal;
	int errVal;

	retVal = QueueUserAPC(ClientLeaveAPCFunc, m_hServerFrameThread, (ULONG_PTR)sessionId);
	if (retVal == FALSE)
	{
		errVal = GetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"QueueUserAPC() ���� : %d", errVal);
	}
}

// lpParam���� SessionId�� �Ѿ��
void CNetServer::ClientLeaveAPCFunc(ULONG_PTR lpParam) noexcept
{
	g_NetServer->OnClientLeave(lpParam);
}
