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

	// ��Ŀ��Ʈ ���� ä���
	USHORT val = MAX_SESSION_COUNT - 1;
	for (int i = 0; i < MAX_SESSION_COUNT; i++)
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
	LINGER ling{ 1, 0 };
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
	m_hIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, IOCP_ACTIVE_THREAD);
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

	// AcceptEx ��û
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
	// listen ���� �ݱ�
	InterlockedExchange(&m_isStop, TRUE);
	closesocket(m_sListenSocket);

	for (int i = 0; i < MAX_SESSION_COUNT; i++)
	{
		if (m_arrPSessions[i] != NULL)
		{
			Disconnect(m_arrPSessions[i]->m_uiSessionID);
		}
	}

	// while SessionCount 0�� ������
	while (true)
	{
		// ���� ���� ����
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

	// ReleaseFlag�� �̹� ���� ��Ȳ
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

// Sector ���� ���� ��쿡�� PQCS
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

	// ReleaseFlag�� �̹� ���� ��Ȳ
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
		header.code = PACKET_KEY; // �ڵ�
		header.len = sBuffer->GetDataSize();
		header.randKey = rand() % 256;
		header.checkSum = CEncryption::CalCheckSum(sBuffer->GetContentBufferPtr(), sBuffer->GetDataSize());
		sBuffer->EnqueueHeader((char *)&header, sizeof(NetHeader));

		// CheckSum ���� ��ȣȭ�ϱ� ����
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

	// Io ���� ����
	CancelIoEx((HANDLE)pSession->m_sSessionSocket, nullptr);

	if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
	{
		// IOCount == 0 �̸� ���� �õ�
		ReleaseSession(pSession, isPQCS);
	}

	return TRUE;
}

BOOL CNetServer::ReleaseSession(CNetSession *pSession, BOOL isPQCS) noexcept
{
	// IoCount üũ�� ReleaseFlag ������ ���ÿ�
	if (InterlockedCompareExchange(&pSession->m_iIOCountAndRelease, CNetSession::RELEASE_FLAG, 0) != 0)
	{
		// ioCount 0�� �ƴϰų� �̹� Release ���� ���ΰ� �ִٸ� return
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
	m_arrAcceptExSessions[index] = newAcceptEx;

	newAcceptEx->m_sSessionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (newAcceptEx->m_sSessionSocket == INVALID_SOCKET)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"PostAcceptEx socket() ���� : %d", errVal);
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
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"setsockopt(SO_UPDATE_ACCEPT_CONTEXT) ���� : %d", errVal);
		return FALSE;
	}

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
	InterlockedIncrement64(&g_monitor.m_lAcceptTotal);
	m_arrPSessions[index] = pSession;

	// ���� �ߴ� ���¸� ���� ����
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
				// ���� ���� ��ƾ
				PostQueuedCompletionStatus(m_hIOCPHandle, 0, 0, 0);
				break;
			}
		}
		// ���� ���� ����
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
				// Accept�� ������ ���� �����͸� ����
				INT index = g_OverlappedAlloc.GetAcceptExIndex((ULONG_PTR)lpOverlapped);
				pSession = m_arrAcceptExSessions[index];

				// �̰� �����ϸ� ���� ����
				// * ���� ������ ��Ȳ - setsockopt, OnConnectionRequest
				// * ioCount ������ 1�� ����
				// * �ٷ� ��� ����
				// * �ٸ� I/O ��û�� �Ȱɰ� ������ ������ �Ʒ��� ioCount 0�� ������ ���� ������ ����
				InterlockedIncrement(&pSession->m_iIOCountAndRelease);
				if (!AcceptExCompleted(pSession))
				{
					// ������ �ε����� ���� ������ �ٽ� �ɾ���
					if (!m_isStop)
						PostAcceptEx(index);
					else
						if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
							CNetSession::Free(pSession);
					break;
				}

				// OnAccept ó���� ���⼭
				OnAccept(pSession->m_uiSessionID);
				// �ش� ���ǿ� ���� Recv ����
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

				// Event Apc Enqueue - PQ ����ȭ ���ϱ� ����
				RegisterTimerEvent(timerEvent);
				continue;
			}
			break;
			case IOOperation::CONTENT_EVENT: // ��ȸ�� �̺�Ʈ
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

// SendFrameThread�� Ȱ��
int CNetServer::TimerEventSchedulerThread() noexcept
{
	// IOCP�� ���༺ ������ �ޱ� ���� GQCS
	DWORD dwTransferred = 0;
	CNetSession *pSession = nullptr;
	OVERLAPPED *lpOverlapped = nullptr;

	GetQueuedCompletionStatus(m_hIOCPHandle, &dwTransferred
		, (PULONG_PTR)&pSession, (LPOVERLAPPED *)&lpOverlapped
		, 0);


	while (m_bIsTimerEventSchedulerRun)
	{
		if (m_TimerEventQueue.empty())
			SleepEx(INFINITE, TRUE); // APC �۾��� Enqueue �Ǹ� ��� ��

		// EventQueue�� ���� ���� ��
		TimerEvent *timerEvent = m_TimerEventQueue.top();
		// ���� ���� ���� �Ǵ�
		LONG dTime = timerEvent->nextExecuteTime - timeGetTime();
		if (dTime <= 0) // 0���� ������ ���� ����
		{
			m_TimerEventQueue.pop();
			PostQueuedCompletionStatus(m_hIOCPHandle, 0, (ULONG_PTR)timerEvent, (LPOVERLAPPED)IOOperation::TIMER_EVENT);
			continue;
		}

		// ���� �ð���ŭ ���
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
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"QueueUserAPC() ���� : %d", errVal);
	}
}

// EventSchedulerThread���� �����
void CNetServer::RegisterTimerEventAPCFunc(ULONG_PTR lpParam) noexcept
{
	g_NetServer->m_TimerEventQueue.push((TimerEvent *)lpParam);
}
