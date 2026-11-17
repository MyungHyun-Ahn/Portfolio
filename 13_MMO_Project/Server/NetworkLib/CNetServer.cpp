#include "pch.h"
#include "DefineServer.h"
#include "COverlappedAllocator.h"
#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CRecvBuffer.h"
#include "CSerializableBufferView.h"
#include "SBaseEvent.h"
#include "ServerSetting.h"
#include "ClientSetting.h"
#include "CMonitor.h"
#include "CNetServer.h"
#include "CLanClient.h"
#include "SystemEvent.h"
#include "CBaseContents.h"
#include "CContentsThread.h"

namespace NetworkLib
{
CNetServer *g_pNetServer = nullptr;

	void CNetSession::RecvCompleted(int size) noexcept
	{
		m_recvBuffer.MoveRear(size);
		int currentUseSize = m_recvBuffer.GetUseSize();
		while (currentUseSize > 0)
		{
			if (currentUseSize < sizeof(SNetHeader))
				break;

			SNetHeader packetHeader;
			m_recvBuffer.Peek((char *)&packetHeader, sizeof(SNetHeader));

			if (packetHeader.code != serverSetting::packetCode)
			{
				g_pNetServer->Disconnect(m_sessionId);
				return;
			}
			if (packetHeader.len > (int)CSerializableBuffer<FALSE>::EBufferDefine::PacketMaxSize - sizeof(SNetHeader)
				|| packetHeader.len >= 65535 || packetHeader.len >= m_recvBuffer.GetCapacity())
			{
				g_pNetServer->Disconnect(m_sessionId);
				return;
			}
			if (m_recvBuffer.GetUseSize() < sizeof(SNetHeader) + packetHeader.len)
				break;

			CSerializableBuffer<FALSE> *pView = CSerializableBuffer<FALSE>::Alloc();
			m_recvBuffer.MoveFront(sizeof(SNetHeader));
			pView->EnqueueHeader((char *)&packetHeader, sizeof(SNetHeader));

			m_recvBuffer.Dequeue(pView->GetContentBufferPtr(), packetHeader.len);
			pView->MoveWritePos(packetHeader.len);

			CEncryption::Decoding(pView->GetBufferPtr() + 4, pView->GetFullSize() - 4, packetHeader.randKey);
			BYTE dataCheckSum = CEncryption::CalCheckSum(pView->GetContentBufferPtr(), pView->GetDataSize());
			BYTE headerCheckSum = *(pView->GetBufferPtr() + offsetof(SNetHeader, checkSum));
			if (dataCheckSum != headerCheckSum)
			{
				CSerializableBuffer<FALSE>::Free(pView);
				g_pNetServer->Disconnect(m_sessionId);
				return;
			}

			pView->IncreaseRef();
			InterlockedIncrement(&g_monitor.m_recvTps);
			m_recvMsgQueue.Enqueue(pView);
			currentUseSize = m_recvBuffer.GetUseSize();
		}
	}

	// m_iSendCount를 믿고 할당 해제를 진행
	// * 논블락킹 I/O일 때만 Send를 요청한 데이터보다 덜 보내는 상황이 발생 가능
	// * 비동기 I/O는 무조건 전부 보내고 완료 통지가 도착함

	void CNetSession::SendCompleted(int size) noexcept
	{
		InterlockedAdd(&g_monitor.m_sendTps, m_sendCount);

		for (int count = 0; count < m_sendCount; count++)
		{
			if (m_pSendBuffers[count]->DecreaseRef() == 0)
			{
				CSerializableBuffer<FALSE>::Free(m_pSendBuffers[count]);
			}
		}

		m_sendCount = 0;

		InterlockedExchange(&m_sendFlag, FALSE);

		PostSend();
	}

	bool CNetSession::PostRecv() noexcept
	{
		int errVal;
		int retVal;

		// 여기서는 수동 Ref 버전이 필요
		// * WSARecv에서 Ref 관리가 불가능

		InterlockedIncrement(&m_ioCountAndRelease);
		if ((m_ioCountAndRelease & CNetSession::sm_releaseFlag) == CNetSession::sm_releaseFlag)
		{
			InterlockedDecrement(&m_ioCountAndRelease);
			return FALSE;
		}

		WSABUF wsaBuf[2];
		wsaBuf[0].buf = m_recvBuffer.GetRearPtr();
		wsaBuf[0].len = m_recvBuffer.DirectEnqueueSize();
		wsaBuf[1].buf = m_recvBuffer.GetPQueuePtr();
		wsaBuf[1].len = m_recvBuffer.GetFreeSize() - wsaBuf[0].len;

		ZeroMemory((m_pMyOverlappedStartAddr + 1), sizeof(OVERLAPPED));

		DWORD flag = 0;
		{
			// PROFILE_BEGIN(0, "WSARecv");
			retVal = WSARecv(m_sessionSocket, wsaBuf, 2, nullptr, &flag, (LPWSAOVERLAPPED)(m_pMyOverlappedStartAddr + 1), NULL);
		}
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			if (errVal != WSA_IO_PENDING)
			{
				if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET && errVal != WSAEINTR)
					g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"WSARecv() Error : %d", errVal);

				// 사실 여기선 0이 될 일이 없음
				// 반환값을 사용안해도 됨
				if (InterlockedDecrement(&m_ioCountAndRelease) == 0)
				{
					return FALSE;
				}
			}
			else
			{
				if (m_cancelIoCalled)
				{
					CancelIoEx((HANDLE)m_sessionSocket, nullptr);
					return FALSE;
				}
			}
		}

		return TRUE;
	}

	bool CNetSession::PostSend(bool isSendFlag) noexcept
	{
		int errVal;
		int retVal;
		int sendUseSize;

		if (!isSendFlag)
		{
			sendUseSize = m_lfSendBufferQueue.GetUseSize();
			if (sendUseSize <= 0)
			{
				return FALSE;
			}

			if ((InterlockedExchange(&m_sendFlag, TRUE) & ~(sm_enqueueFlag)) == TRUE)
			{
				return FALSE;
			}
		}

		sendUseSize = m_lfSendBufferQueue.GetUseSize();
		if (sendUseSize <= 0)
		{
			LONG beforeSendFlag = InterlockedCompareExchange(&m_sendFlag, FALSE, TRUE);
			if ((beforeSendFlag & sm_enqueueFlag) == sm_enqueueFlag) // 최상위 비트 켜졌으면
			{
				PostSend(TRUE);
				return FALSE;
			}
			return FALSE;
		}

		InterlockedIncrement(&m_ioCountAndRelease);
		if ((m_ioCountAndRelease & CNetSession::sm_releaseFlag) == CNetSession::sm_releaseFlag)
		{
			InterlockedDecrement(&m_ioCountAndRelease);
			InterlockedExchange(&m_sendFlag, FALSE);
			return FALSE;
		}

		WSABUF wsaBuf[kWsasendMaxBufferCount];

		m_sendCount = min(sendUseSize, kWsasendMaxBufferCount);
		// WSASEND_MAX_BUFFER_COUNT 만큼 1초에 몇번 보내는지 카운트
		// 이 수치가 높다면 더 늘릴 것
		if (m_sendCount == kWsasendMaxBufferCount)
			InterlockedIncrement(&g_monitor.m_maxSendCount);

		int count;
		for (count = 0; count < m_sendCount; count++)
		{
			CSerializableBuffer<FALSE> *pSBuffer;
			// 못꺼낸 것
			if (!m_lfSendBufferQueue.Dequeue(&pSBuffer))
			{
				MHLib::utils::g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::ELogLevel::Error, L"LFQueue::Dequeue() Error");
				// 말도 안되는 상황
				MHLib::debug::CCrashDump::Crash();
			}

			if (!pSBuffer->GetIsEnqueueHeader())
			{
				pSBuffer->m_isEnqueueHeader = true;
				SNetHeader *pHeader = (SNetHeader *)pSBuffer->GetBufferPtr();
				pHeader->code = serverSetting::packetCode; // 코드
				pHeader->len = pSBuffer->GetDataSize();
				pHeader->randKey = 0;
				pHeader->checkSum = MHLib::security::CEncryption::CalCheckSum(pSBuffer->GetContentBufferPtr(), pSBuffer->GetDataSize());
			
				// CheckSum 부터 암호화하기 위해
				CEncryption::Encoding(pSBuffer->GetBufferPtr() + 4, pSBuffer->GetDataSize() + 1, pHeader->randKey);
			}

			wsaBuf[count].buf = pSBuffer->GetBufferPtr();
			wsaBuf[count].len = pSBuffer->GetFullSize();

			m_pSendBuffers[count] = pSBuffer;
		}

		InterlockedAnd(&m_sendFlag, TRUE);

		ZeroMemory((m_pMyOverlappedStartAddr + 2), sizeof(OVERLAPPED));
		InterlockedIncrement(&g_monitor.m_sendCallCount);
		{
			PROFILE_BEGIN(0, "WSASend");
			retVal = WSASend(m_sessionSocket, wsaBuf, m_sendCount, nullptr, 0, (LPOVERLAPPED)(m_pMyOverlappedStartAddr + 2), NULL);
		}
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			if (errVal != WSA_IO_PENDING)
			{
				if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET && errVal != WSAEINTR)
					g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"WSASend() Error : %d", errVal);

				// 사실 여기선 0이 될 일이 없음
				// 반환값을 사용안해도 됨
				if (InterlockedDecrement(&m_ioCountAndRelease) == 0)
				{
					return FALSE;
				}
			}
			else
			{
				InterlockedIncrement(&g_monitor.m_sendPendingCount);
				if (m_cancelIoCalled)
				{
					CancelIoEx((HANDLE)m_sessionSocket, nullptr);
					return FALSE;
				}
			}
		}

		return TRUE;
	}

	void CNetSession::Clear() noexcept
	{
		for (int count = 0; count < m_sendCount; count++)
		{
			if (m_pSendBuffers[count]->DecreaseRef() == 0)
			{
				CSerializableBuffer<FALSE>::Free(m_pSendBuffers[count]);
			}
		}

		m_sendCount = 0;

		LONG useBufferSize = m_lfSendBufferQueue.GetUseSize();
		for (int i = 0; i < useBufferSize; i++)
		{
			CSerializableBuffer<FALSE> *pBuffer;
			// 못꺼낸 것
			if (!m_lfSendBufferQueue.Dequeue(&pBuffer))
			{
				MHLib::utils::g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::ELogLevel::Error, L"LFQueue::Dequeue() Error");
				// 말도 안되는 상황
				CCrashDump::Crash();
			}

			// RefCount를 낮추고 0이라면 보낸 거 삭제
			if (pBuffer->DecreaseRef() == 0)
			{
				CSerializableBuffer<FALSE>::Free(pBuffer);
			}
		}

		useBufferSize = m_lfSendBufferQueue.GetUseSize();
		if (useBufferSize != 0)
		{
			g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"LFQueue is not empty Error");
		}

		useBufferSize = m_recvMsgQueue.GetUseSize();
		for (int i = 0; i < useBufferSize; i++)
		{
			CSerializableBuffer<FALSE> *pBuffer;
			if (!m_recvMsgQueue.Dequeue(&pBuffer))
			{
				CCrashDump::Crash();
			}

			if (pBuffer->DecreaseRef() == 0)
				CSerializableBuffer<FALSE>::Free(pBuffer);
		}

		m_sessionSocket = INVALID_SOCKET;
		m_sessionId = 0;
		// m_pRecvBuffer = nullptr;
		m_pCurrentContent = nullptr;
		m_recvBuffer.Clear();
		InterlockedExchange(&m_sendFlag, FALSE);
	}

	unsigned int NetWorkerThreadFunc(LPVOID pParam) noexcept
	{
		return g_pNetServer->WorkerThread();
	}

	BOOL CNetServer::Start(const CHAR *pOpenIp, const USHORT port) noexcept
	{
		int retVal;
		int errVal;
		WSAData wsaData;

		m_maxSessionCount = serverSetting::maxSessionCount;

		m_pSessions = new CNetSession * [serverSetting::maxSessionCount];

		ZeroMemory((char *)m_pSessions, sizeof(CNetSession *) * serverSetting::maxSessionCount);

		m_pAcceptExSessions = new CNetSession * [serverSetting::acceptexCount];

		// 디스커넥트 스택 채우기
		USHORT val = serverSetting::maxSessionCount - 1;
		for (int i = 0; i < serverSetting::maxSessionCount; i++)
		{
			m_stackDisconnectIndex.Push(val--);
		}

		retVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (retVal != 0)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"WSAStartup() 실패 : %d", errVal);
			return FALSE;
		}

		m_listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
		if (m_listenSocket == INVALID_SOCKET)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"WSASocket() 실패 : %d", errVal);
			return FALSE;
		}

		SOCKADDR_IN serverAddr;
		ZeroMemory(&serverAddr, sizeof(serverAddr));
		serverAddr.sin_family = AF_INET;
		InetPtonA(AF_INET, pOpenIp, &serverAddr.sin_addr);
		serverAddr.sin_port = htons(port);

		retVal = bind(m_listenSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"bind() 실패 : %d", errVal);
			return FALSE;
		}

		// 송신 버퍼를 0으로 만들어서 실직적인 I/O를 우리의 버퍼를 이용하도록 만듬
		if (serverSetting::useZeroCopy)
		{
			DWORD sendBufferSize = 0;
			retVal = setsockopt(m_listenSocket, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufferSize, sizeof(DWORD));
			if (retVal == SOCKET_ERROR)
			{
				errVal = WSAGetLastError();
				g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"setsockopt(SO_SNDBUF) 실패 : %d", errVal);
				return FALSE;
			}
		}

		// LINGER option 설정
		LINGER ling{ 1, 0 };
		retVal = setsockopt(m_listenSocket, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof(ling));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"setsockopt(SO_LINGER) 실패 : %d", errVal);
			return FALSE;
		}

		// listen
		retVal = listen(m_listenSocket, SOMAXCONN_HINT(65535));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"listen() 실패 : %d", errVal);
			return FALSE;
		}

		// CP 핸들 생성
		m_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, serverSetting::iocpActiveThread);
		if (m_iocpHandle == NULL)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"CreateIoCompletionPort(생성) 실패 : %d", errVal);
			return FALSE;
		}

		CreateIoCompletionPort((HANDLE)m_listenSocket, m_iocpHandle, (ULONG_PTR)0, 0);

		DWORD dwBytes;
		retVal = WSAIoctl(m_listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidAcceptEx, sizeof(m_guidAcceptEx),
			&m_pAcceptEx, sizeof(m_pAcceptEx), &dwBytes, NULL, NULL);
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"WSAIoctl(lpfnAcceptEx) 실패 : %d", errVal);
			return FALSE;
		}

		retVal = WSAIoctl(m_listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidGetAcceptExSockaddrs,
			sizeof(m_guidGetAcceptExSockaddrs), &m_pGetAcceptExSockaddrs, sizeof(m_pGetAcceptExSockaddrs), &dwBytes, NULL, NULL);
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"WSAIoctl(lpfnGetAcceptExSockaddrs) 실패 : %d", errVal);
			return FALSE;
		}

		// AcceptEx 요청
		FristPostAcceptEx();

		for (int i = 0; i < serverSetting::contentsThreadCount; i++)
		{
			CContentsThread *pContentsThread = new CContentsThread;
			pContentsThread->Create();
		}

		CContentsThread::RunAll();

		// CreateWorkerThread
		for (int i = 1; i <= serverSetting::iocpWorkerThread; i++)
		{
			HANDLE hWorkerThread = (HANDLE)_beginthreadex(nullptr, 0, NetWorkerThreadFunc, nullptr, 0, nullptr);
			if (hWorkerThread == 0)
			{
				errVal = GetLastError();
				g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"WorkerThread[%d] running fail.. : %d", i, errVal);
				return FALSE;
			}

			m_arrWorkerThreads.push_back(hWorkerThread);
			g_pLogger->WriteLogConsole(ELogLevel::System, L"[SYSTEM] WorkerThread[%d] running..", i);
		}

		RegisterSystemTimerEvent();
		RegisterContentTimerEvent();

		WaitForMultipleObjects((DWORD)m_arrWorkerThreads.size(), m_arrWorkerThreads.data(), TRUE, INFINITE);

		return TRUE;
	}

	void CNetServer::Stop()
	{
		// listen 소켓 닫기
		InterlockedExchange(&m_isStop, TRUE);
		closesocket(m_listenSocket);

		for (int i = 0; i < serverSetting::maxSessionCount; i++)
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
			if (m_sessionCount == 0)
			{
				m_isWorkerRun = FALSE;
				PostQueuedCompletionStatus(m_iocpHandle, 0, 0, 0);
				break;
			}
		}
	}

	void CNetServer::SendPacket(const UINT64 sessionId, CSerializableBuffer<FALSE> *pSBuffer) noexcept
	{
		CNetSession *pSession = m_pSessions[getIndex(sessionId)];

		InterlockedIncrement(&pSession->m_ioCountAndRelease);
		if ((pSession->m_ioCountAndRelease & CNetSession::sm_releaseFlag) == CNetSession::sm_releaseFlag)
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

	// Sector 락이 잡힌 경우에만 PQCS
	void CNetServer::EnqueuePacket(const UINT64 sessionId, CSerializableBuffer<FALSE> *pSBuffer) noexcept
	{
		CNetSession *pSession = m_pSessions[getIndex(sessionId)];

		InterlockedIncrement(&pSession->m_ioCountAndRelease);
		// ReleaseFlag가 이미 켜진 상황
		if ((pSession->m_ioCountAndRelease & CNetSession::sm_releaseFlag) == CNetSession::sm_releaseFlag)
		{
			if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
			{
				ReleaseSession(pSession, TRUE);
			}
			return;
		}

		if (sessionId != pSession->m_sessionId)
		{
			if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
			{
				ReleaseSession(pSession, TRUE);
			}
			return;
		}

		if (!pSession->SendPacket(pSBuffer))
			Disconnect(sessionId);

		if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
		{
			ReleaseSession(pSession, TRUE);
		}
	}

	void CNetServer::SendPqcs(const CNetSession *pSession)
	{
		PostQueuedCompletionStatus(m_iocpHandle, 0, (ULONG_PTR)pSession, (LPOVERLAPPED)EIoOperation::SendPost);
	}

	void CNetServer::SendAll()
	{
		for (int i = 0; i < serverSetting::maxSessionCount; i++)
		{
			if (m_pSessions[i] != nullptr)
				m_pSessions[i]->PostSend();
		}

		InterlockedExchange(&m_sendAllFlag, FALSE);
	}

	void CNetServer::SendAllPqcs()
	{
		if (InterlockedExchange(&m_sendAllFlag, TRUE) == FALSE)
			PostQueuedCompletionStatus(m_iocpHandle, 0, 0, (LPOVERLAPPED)EIoOperation::SendAll);
	}

	BOOL CNetServer::Disconnect(const UINT64 sessionId, BOOL isPqcs) noexcept
	{
		CNetSession *pSession = m_pSessions[getIndex(sessionId)];

		// ReleaseFlag가 이미 켜진 상황
		InterlockedIncrement(&pSession->m_ioCountAndRelease);
		if ((pSession->m_ioCountAndRelease & CNetSession::sm_releaseFlag) == CNetSession::sm_releaseFlag)
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

	BOOL CNetServer::ReleaseSession(CNetSession *pSession, BOOL isPqcs) noexcept
	{
		if (InterlockedCompareExchange(&pSession->m_ioCountAndRelease, CNetSession::sm_releaseFlag, 0) != 0)
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
	
		if (pSession->m_pCurrentContent != nullptr)
			pSession->m_pCurrentContent->LeaveJobEnqueue(freeSessionId);
		OnClientLeave(freeSessionId);

		CNetSession::Free(pSession);
		InterlockedDecrement(&m_sessionCount);
		m_stackDisconnectIndex.Push(index);

		return TRUE;
	}

	BOOL CNetServer::ReleaseSessionPqcs(CNetSession *pSession) noexcept
	{
		USHORT index = getIndex(pSession->m_sessionId);
		closesocket(pSession->m_sessionSocket);
		UINT64 freeSessionId = pSession->m_sessionId;

		OnClientLeave(freeSessionId);

		if (pSession->m_pCurrentContent != nullptr)
			pSession->m_pCurrentContent->LeaveJobEnqueue(freeSessionId);

		CNetSession::Free(pSession);
		InterlockedDecrement(&m_sessionCount);
		m_stackDisconnectIndex.Push(index);

		return TRUE;
	}

	void CNetServer::FristPostAcceptEx() noexcept
	{
		// 처음에 AcceptEx를 걸어두고 시작
		for (int i = 0; i < serverSetting::acceptexCount; i++)
		{
			PostAcceptEx(i);
		}
	}

	BOOL CNetServer::PostAcceptEx(INT index) noexcept
	{
		int retVal;
		int errVal;

		CNetSession *pNewAcceptEx = CNetSession::Alloc();
		m_pAcceptExSessions[index] = pNewAcceptEx;

		pNewAcceptEx->m_sessionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (pNewAcceptEx->m_sessionSocket == INVALID_SOCKET)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"PostAcceptEx socket() 실패 : %d", errVal);
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
				if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET)
					g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"AcceptEx() Error : %d", errVal);

				return FALSE;
			}
		}

		return TRUE;
	}

	BOOL CNetServer::AcceptExCompleted(CNetSession *pSession) noexcept
	{
		InterlockedIncrement(&g_monitor.m_acceptTps);

		int retVal;
		int errVal;
		retVal = setsockopt(pSession->m_sessionSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
			(char *)&m_listenSocket, sizeof(SOCKET));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();

			if (!m_isStop)
				g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"setsockopt(SO_UPDATE_ACCEPT_CONTEXT) 실패 : %d", errVal);
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
			g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"m_stackDisconnectIndex.Pop(&index) failed");
			return FALSE;
		}

		UINT64 combineId = CNetServer::combineIndex(index, InterlockedIncrement64(&m_currentId));

		pSession->Init(combineId);

		InterlockedIncrement(&m_sessionCount);
		InterlockedIncrement64(&g_monitor.m_acceptTotal);
		m_pSessions[index] = pSession;

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
		while (m_isWorkerRun)
		{
			DWORD dwTransferred = 0;
			CNetSession *pSession = nullptr;
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
					// GetQueuedCompletionStatus Fail
					break;
				}

				if (dwTransferred == 0 && pSession == 0)
				{
					// 정상 종료 루틴
					PostQueuedCompletionStatus(m_iocpHandle, 0, 0, 0);
					break;
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
						CNetSession::Free(pSession);
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
					pSession->RecvCompleted(dwTransferred);
					pSession->PostRecv();
				}
				break;
				case EIoOperation::Send:
				{
					pSession->SendCompleted(dwTransferred);
				}
				break;
				case EIoOperation::SendPost:
				{
					pSession->PostSend();
					continue;
				}
				break;
				case EIoOperation::SendAll:
				{
					SendAll();
					continue;
				}
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

	void CNetServer::RegisterSystemTimerEvent()
	{
		SMonitorTimerEvent *pMonitorEvent = new SMonitorTimerEvent;
		pMonitorEvent->SetEvent();
		CContentsThread::EnqueueEvent(pMonitorEvent);
		// CContentThread::s_arrContentThreads[2]->EnqueueEventMy(pMonitorEvent);

		SKeyboardTimerEvent *pKeyBoardEvent = new SKeyboardTimerEvent;
		pKeyBoardEvent->SetEvent();
		CContentsThread::EnqueueEvent(pKeyBoardEvent);
		// CContentThread::s_arrContentThreads[2]->EnqueueEventMy(pKeyBoardEvent);

		// SendAllTimerEvent *pSendAllEvent = new SendAllTimerEvent;
		// pSendAllEvent->SetEvent();
		// CContentThread::s_arrContentThreads[3]->EnqueueEventMy(pSendAllEvent);
	}
}
