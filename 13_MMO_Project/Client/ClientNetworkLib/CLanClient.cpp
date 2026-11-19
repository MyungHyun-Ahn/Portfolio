#include "pch.h"
#include "SNetworkCounters.h"
#include "NetworkLimits.h"
#include "NetHeaders.h"
#include "COverlappedAllocator.h"
#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CRecvBuffer.h"
#include "CSerializableBufferView.h"
#include "ClientSetting.h"
#include "CLanClient.h"

namespace NetworkLib
{
CLanClientManager *g_pLanClientManager;

	void CLanSession::Clear() noexcept
	{
		m_spDelayedBuffer.reset();
		for (int count = 0; count < m_sendCount; ++count)
			m_spSendBuffers[count].reset();
		m_sendCount = 0;
		m_sendQueue.clear();
		m_spRecvBuffer.reset();
	}

	void CLanSession::RecvCompleted(int size) noexcept
	{
		m_spRecvBuffer->MoveRear(size);
		auto failReceive = [this]
		{
			m_spDelayedBuffer.reset();
			m_spRecvBuffer.reset();
			g_pLanClientManager->m_arrNetClients[m_clientMgrIndex]->Disconnect(m_sessionId);
		};

		// 이전에서 딜레이 된게 있는 경우
		if (m_spDelayedBuffer != nullptr)
		{
			// 딜레이 된걸 처리해야함
			USHORT delayedHeaderSize = m_spDelayedBuffer->isReadHeaderSize();
			// 헤더 사이즈 부족
			if (delayedHeaderSize < (int)CSerializableBuffer<TRUE>::EBufferDefine::HeaderSize)
			{
				// 헤더가 딜레이 됐나?
				// 딜레이 됐다면 여기에 모자른 만큼써줌
				int requireHeaderSize = (int)CSerializableBuffer<TRUE>::EBufferDefine::HeaderSize - delayedHeaderSize;
				// 헤더가 2번씩 밀릴일은 없을 것임
				// 짤린 헤더를 써주고
				m_spDelayedBuffer->writeDelayedHeader(m_spRecvBuffer->GetFrontPtr(), requireHeaderSize);
				m_spRecvBuffer->MoveFront(requireHeaderSize);
				// 네트워크 헤더 파악
				// - 지금은 USHORT 나중에 헤더로 정의하면 여기를 변경
				SLanHeader header;
				m_spDelayedBuffer->getDelayedHeader((char *)&header, sizeof(SLanHeader));

				// 여기서 버퍼를 할당
				// - 헤더 사이즈 포함
				if (!m_spDelayedBuffer->tryInitHeapStorage(header.len + sizeof(SLanHeader)))
				{
					failReceive();
					return;
				}
				m_spDelayedBuffer->copy((char *)&header, 0, sizeof(SLanHeader));
			}

			// header는 다 읽힌 것
			SLanHeader header;
			m_spDelayedBuffer->getHeader((char *)&header, sizeof(SLanHeader));

			if (header.len >= 65535 || header.len >= m_spRecvBuffer->GetCapacity())
			{
				failReceive();
				return;
			}

			int requireSize = header.len - m_spDelayedBuffer->GetDataSize();
			// 데이터 모자름
			int recvBufferUseSize = m_spRecvBuffer->GetUseSize();
			if (recvBufferUseSize < requireSize)
			{
				m_spDelayedBuffer->copy(m_spRecvBuffer->GetFrontPtr(), recvBufferUseSize);
				m_spRecvBuffer->MoveFront(recvBufferUseSize);
			}
			else
			{
				// 필요한만큼 쓸 수 있으면
				m_spDelayedBuffer->copy(m_spRecvBuffer->GetFrontPtr(), requireSize);
				m_spRecvBuffer->MoveFront(requireSize);

				InterlockedIncrement(&g_networkCounters.m_recvTps);
				m_spDelayedBuffer->m_sessionId = m_sessionId;
				g_pLanClientManager->m_arrNetClients[m_clientMgrIndex]->OnRecv(m_sessionId, std::move(m_spDelayedBuffer));
			}
		}

		DWORD currentUseSize = m_spRecvBuffer->GetUseSize();
		while (currentUseSize > 0)
		{
			SLanHeader packetHeader;
			std::shared_ptr<CSerializableBufferView<TRUE>> spView;
			try
			{
				spView = std::make_shared<CSerializableBufferView<TRUE>>();
			}
			catch (const std::bad_alloc &)
			{
				failReceive();
				return;
			}

			if (currentUseSize < (int)CSerializableBuffer<TRUE>::EBufferDefine::HeaderSize)
			{
				// Peek 실패 delayedBuffer로

				int remainSize = m_spRecvBuffer->GetUseSize();
				// 남은 사이즈 만큼 씀
				spView->writeDelayedHeader(m_spRecvBuffer->GetFrontPtr(), remainSize);
				m_spRecvBuffer->MoveFront(remainSize);
				m_spDelayedBuffer = std::move(spView);

				// g_Logger->WriteLog(L"RecvBuffer", LOG_LEVEL::DEBUG, L"HeaderDelay");
				break;
			}

			int useSize = m_spRecvBuffer->GetUseSize();
			m_spRecvBuffer->Peek((char *)&packetHeader, (int)CSerializableBuffer<TRUE>::EBufferDefine::HeaderSize);

			if (packetHeader.len >= 65535 || packetHeader.len >= m_spRecvBuffer->GetCapacity())
			{
				failReceive();
				return;
			}

			if (useSize < (int)CSerializableBuffer<TRUE>::EBufferDefine::HeaderSize + packetHeader.len)
			{
				// 직접 할당해서 뒤로 밀기 Delayed
				// 헤더는 읽은 것임
				// front 부터 끝까지 복사
				if (!spView->tryInitHeapStorage(packetHeader.len + sizeof(SLanHeader)))
				{
					failReceive();
					return;
				}
				spView->copy(m_spRecvBuffer->GetFrontPtr(), 0, useSize);
				m_spRecvBuffer->MoveFront(useSize);
				m_spDelayedBuffer = std::move(spView);

				// g_Logger->WriteLog(L"RecvBuffer", LOG_LEVEL::DEBUG, L"PacketDelay");
				break;
			}

			// 실질적으로 Recv 버퍼 입장에서는 읽은 것
			// - 하나의 메시지가 완전히 완성된 상태
			int offsetStart = m_spRecvBuffer->GetFrontOffset();
			int offsetEnd = offsetStart + packetHeader.len + (int)CSerializableBuffer<TRUE>::EBufferDefine::HeaderSize;

			// 완성된 뷰가 수신 저장소를 공유하므로 콜백 이후에도 보관할 수 있다.
			spView->init(m_spRecvBuffer, offsetStart, offsetEnd);
			m_spRecvBuffer->MoveFront((int)CSerializableBuffer<TRUE>::EBufferDefine::HeaderSize + packetHeader.len);

			InterlockedIncrement(&g_networkCounters.m_recvTps);
			spView->m_sessionId = m_sessionId;
			g_pLanClientManager->m_arrNetClients[m_clientMgrIndex]->OnRecv(m_sessionId, std::move(spView));

			currentUseSize = m_spRecvBuffer->GetUseSize();
		}

		m_spRecvBuffer.reset();
	}

	bool CLanSession::SendPacket(std::shared_ptr<CSerializableBuffer<TRUE>> spMessage) noexcept
	{
		std::lock_guard sendStateLock(m_sendStateLock);
		try
		{
			m_sendQueue.push_back(std::move(spMessage));
		}
		catch (const std::bad_alloc &)
		{
			return false;
		}
		return TRUE;
	}

	void CLanSession::SendCompleted(int size) noexcept
	{
		{
			std::lock_guard sendStateLock(m_sendStateLock);
			InterlockedAdd(&g_networkCounters.m_sendTps, m_sendCount);

			for (int count = 0; count < m_sendCount; ++count)
				m_spSendBuffers[count].reset();
			m_sendCount = 0;
		}

		// SendFlag를 풀지 않고 진행
		PostSend(TRUE);
	}

	bool CLanSession::PostRecv() noexcept
	{
		if (InterlockedCompareExchange(&m_cancelIoCalled, FALSE, FALSE))
			return FALSE;
		int errVal;
		int retVal;

		// 수신 완료를 소비할 때까지 세션이 저장소를 소유한다.

		InterlockedIncrement(&m_ioCountAndRelease);
		if ((m_ioCountAndRelease & CLanSession::sm_releaseFlag) == CLanSession::sm_releaseFlag)
		{
			InterlockedDecrement(&m_ioCountAndRelease);
			return FALSE;
		}

		m_spRecvBuffer = CRecvBuffer::MakeShared();
		if (m_spRecvBuffer == nullptr)
		{
			InterlockedDecrement(&m_ioCountAndRelease);
			return FALSE;
		}
		WSABUF wsaBuf;
		wsaBuf.buf = m_spRecvBuffer->GetPQueuePtr();
		wsaBuf.len = m_spRecvBuffer->GetCapacity();

		ZeroMemory((&m_overlappeds[1]), sizeof(OVERLAPPED));

		DWORD flag = 0;
		{
			// PROFILE_BEGIN(0, "WSARecv");
			retVal = WSARecv(m_sessionSocket, &wsaBuf, 1, nullptr, &flag, (LPWSAOVERLAPPED)(&m_overlappeds[1]), NULL);
		}
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			if (errVal != WSA_IO_PENDING)
			{
				if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET && errVal != WSAEINTR)
					g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"WSARecv() Error : %d", errVal);

				m_spRecvBuffer.reset();
				InterlockedDecrement(&m_ioCountAndRelease);
				return FALSE;
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

	bool CLanSession::PostSend(bool isPqcs) noexcept
	{
		int errVal;
		int retVal;
		std::lock_guard sendStateLock(m_sendStateLock);
		if (!isPqcs && m_sendFlag)
			return FALSE;
		if (m_sendQueue.empty() || InterlockedCompareExchange(&m_cancelIoCalled, FALSE, FALSE))
		{
			m_sendFlag = FALSE;
			return FALSE;
		}
		m_sendFlag = TRUE;

		InterlockedIncrement(&m_ioCountAndRelease);
		if ((m_ioCountAndRelease & CLanSession::sm_releaseFlag) == CLanSession::sm_releaseFlag)
		{
			InterlockedDecrement(&m_ioCountAndRelease);
			InterlockedExchange(&m_sendFlag, FALSE);
			return FALSE;
		}

		WSABUF wsaBuf[kWsasendMaxBufferCount];

		m_sendCount = static_cast<int>((std::min)(m_sendQueue.size(), static_cast<size_t>(kWsasendMaxBufferCount)));
		// WSASEND_MAX_BUFFER_COUNT 만큼 1초에 몇번 보내는지 카운트
		// 이 수치가 높다면 더 늘릴 것
		if (m_sendCount == kWsasendMaxBufferCount)
			InterlockedIncrement(&g_networkCounters.m_maxSendCount);

		int count;
		for (count = 0; count < m_sendCount; count++)
		{
			auto spBuffer = std::move(m_sendQueue.front());
			m_sendQueue.pop_front();

			wsaBuf[count].buf = spBuffer->GetBufferPtr();
			wsaBuf[count].len = spBuffer->GetFullSize();

			m_spSendBuffers[count] = std::move(spBuffer);
		}

		ZeroMemory((&m_overlappeds[2]), sizeof(OVERLAPPED));

		{
			// PROFILE_BEGIN(0, "WSASend");
			retVal = WSASend(m_sessionSocket, wsaBuf, m_sendCount, nullptr, 0, (LPOVERLAPPED)(&m_overlappeds[2]), NULL);
		}
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			if (errVal != WSA_IO_PENDING)
			{
				if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET && errVal != WSAEINTR)
					MHLib::utils::g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::ELogLevel::Error,
						L"WSASend() Error : %d", errVal);

				InterlockedDecrement(&m_ioCountAndRelease);
				InterlockedExchange(&m_cancelIoCalled, TRUE);
				CancelIoEx(reinterpret_cast<HANDLE>(m_sessionSocket), nullptr);
				return FALSE;
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

	// NetClient
	// 서버 연결 준비 작업
	BOOL CLanClient::Init(INT maxSessionCount) noexcept
	{
		if (maxSessionCount <= 0 || maxSessionCount > 65536)
			return FALSE;
		try
		{
			std::vector<USHORT> freeIndices;
			std::vector<std::shared_ptr<CLanSession>> sessions(maxSessionCount);
			freeIndices.reserve(maxSessionCount);
			for (int i = maxSessionCount; i > 0; --i)
				freeIndices.push_back(static_cast<USHORT>(i - 1));
			std::lock_guard sessionTableLock(m_sessionTableLock);
			if (!m_sessions.empty())
				return FALSE;
			m_freeIndices.swap(freeIndices);
			m_sessions.swap(sessions);
		}
		catch (const std::bad_alloc &)
		{
			return FALSE;
		}

		return TRUE;
	}

	std::shared_ptr<CLanSession> CLanClient::acquireSession(UINT64 sessionId) noexcept
	{
		std::lock_guard sessionTableLock(m_sessionTableLock);
		const size_t index = getIndex(sessionId);
		if (index >= m_sessions.size() || m_sessions[index] == nullptr || m_sessions[index]->m_sessionId != sessionId)
			return {};
		auto spSession = m_sessions[index];
		const LONG ioCount = InterlockedIncrement(&spSession->m_ioCountAndRelease);
		if (ioCount & CLanSession::sm_releaseFlag)
		{
			InterlockedDecrement(&spSession->m_ioCountAndRelease);
			return {};
		}
		return spSession;
	}

	void CLanClient::SendPacket(const UINT64 sessionId, std::shared_ptr<CSerializableBuffer<TRUE>> spBuffer) noexcept
	{
		if (spBuffer == nullptr || !spBuffer->IsHeapAllocated())
			return;
		auto spSession = acquireSession(sessionId);
		if (spSession == nullptr)
			return;
		auto *pSession = spSession.get();

		if (!spBuffer->GetIsEnqueueHeader())
		{
			SLanHeader header;
			header.len = spBuffer->GetDataSize();
			spBuffer->EnqueueHeader((char *)&header, sizeof(SLanHeader));
		}

		if (pSession->SendPacket(std::move(spBuffer)))
			pSession->PostSend();
		else
			Disconnect(sessionId);

		if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
		{
			ReleaseSession(pSession);
		}
	}

	void CLanClient::EnqueuePacket(const UINT64 sessionId, std::shared_ptr<CSerializableBuffer<TRUE>> spBuffer) noexcept
	{
		if (spBuffer == nullptr || !spBuffer->IsHeapAllocated())
			return;
		auto spSession = acquireSession(sessionId);
		if (spSession == nullptr)
			return;
		auto *pSession = spSession.get();

		if (!spBuffer->GetIsEnqueueHeader())
		{
			SLanHeader header;
			header.len = spBuffer->GetDataSize();
			spBuffer->EnqueueHeader((char *)&header, sizeof(SLanHeader));
		}

		if (!pSession->SendPacket(std::move(spBuffer)))
			Disconnect(sessionId, TRUE);

		if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
		{
			ReleaseSession(pSession, TRUE);
		}
	}

	BOOL CLanClient::Disconnect(const UINT64 sessionId, BOOL isPqcs) noexcept
	{
		auto spSession = acquireSession(sessionId);
		if (spSession == nullptr)
			return FALSE;
		auto *pSession = spSession.get();

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

	BOOL CLanClient::ReleaseSession(CLanSession *pSession, BOOL isPqcs) noexcept
	{
		// IoCount 체크와 ReleaseFlag 변경을 동시에
		if (InterlockedCompareExchange(&pSession->m_ioCountAndRelease, CLanSession::sm_releaseFlag, 0) != 0)
		{
			// ioCount 0이 아니거나 이미 Release 진행 중인게 있다면 return
			return FALSE;
		}

		if (isPqcs && PostQueuedCompletionStatus(g_pLanClientManager->m_iocpHandle, 0, reinterpret_cast<ULONG_PTR>(pSession),
			reinterpret_cast<LPOVERLAPPED>(EIoOperation::ReleaseSession)))
			return TRUE;
		return ReleaseSessionPqcs(pSession);
	}

	BOOL CLanClient::ReleaseSessionPqcs(CLanSession *pSession) noexcept
	{
		retireSession(*pSession, pSession->m_isConnected);
		return TRUE;
	}

	void CLanClient::retireSession(CLanSession &session, bool isNotify) noexcept
	{
		const UINT64 sessionId = session.m_sessionId;
		const USHORT index = getIndex(sessionId);
		std::shared_ptr<CLanSession> spSession;
		{
			std::lock_guard sessionTableLock(m_sessionTableLock);
			spSession = std::move(m_sessions[index]);
		}
		closesocket(session.m_sessionSocket);
		session.Clear();
		InterlockedDecrement(&m_sessionCount);
		if (isNotify)
			OnDisconnect(sessionId);
		{
			std::lock_guard sessionTableLock(m_sessionTableLock);
			m_freeIndices.push_back(index);
		}
	}

	BOOL CLanClient::PostConnectEx(const CHAR *pIp, const USHORT port) noexcept
	{
		int retVal;
		int errVal;

		std::shared_ptr<CLanSession> spSession;
		try
		{
			spSession = std::make_shared<CLanSession>();
		}
		catch (const std::bad_alloc &)
		{
			return FALSE;
		}
		auto *pConnectSession = spSession.get();
		pConnectSession->m_sessionSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
		if (pConnectSession->m_sessionSocket == INVALID_SOCKET)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"PostConnectEx socket() 실패 : %d", errVal);
			return FALSE;
		}

		// 송신 버퍼를 0으로 만들어서 실직적인 I/O를 우리의 버퍼를 이용하도록 만듬
		if (clientSetting::useZeroCopy)
		{
			DWORD sendBufferSize = 0;
			retVal = setsockopt(pConnectSession->m_sessionSocket, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufferSize, sizeof(DWORD));
			if (retVal == SOCKET_ERROR)
			{
				errVal = WSAGetLastError();
				g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"setsockopt(SO_SNDBUF) 실패 : %d", errVal);
				closesocket(pConnectSession->m_sessionSocket);
				return FALSE;
			}
		}

		// LINGER option 설정
		LINGER ling{ 1, 0 };
		retVal = setsockopt(pConnectSession->m_sessionSocket, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof(ling));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"setsockopt(SO_LINGER) 실패 : %d", errVal);
			closesocket(pConnectSession->m_sessionSocket);
			return FALSE;
		}

		SOCKADDR_IN clientAddr;
		ZeroMemory(&clientAddr, sizeof(clientAddr));
		clientAddr.sin_family = AF_INET;
		clientAddr.sin_addr.s_addr = INADDR_ANY;
		clientAddr.sin_port = 0;

		retVal = bind(pConnectSession->m_sessionSocket, (SOCKADDR *)&clientAddr, sizeof(clientAddr));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"bind() 실패 : %d", errVal);
			closesocket(pConnectSession->m_sessionSocket);
			return FALSE;
		}

		{
			std::lock_guard sessionTableLock(m_sessionTableLock);
			if (m_freeIndices.empty())
			{
				closesocket(pConnectSession->m_sessionSocket);
				return FALSE;
			}
			const USHORT index = m_freeIndices.back();
			m_freeIndices.pop_back();
			pConnectSession->Init(combineIndex(index, ++m_currentId), m_clientMgrIndex);
			InterlockedIncrement(&pConnectSession->m_ioCountAndRelease);
			m_sessions[index] = spSession;
			InterlockedIncrement(&m_sessionCount);
		}

		if (!g_pLanClientManager->RegisterIoCompletionPort(pConnectSession->m_sessionSocket,
			reinterpret_cast<ULONG_PTR>(pConnectSession)))
		{
			if (InterlockedDecrement(&pConnectSession->m_ioCountAndRelease) == 0)
				ReleaseSession(pConnectSession);
			return FALSE;
		}

		SOCKADDR_IN serverAddr;
		ZeroMemory(&serverAddr, sizeof(serverAddr));
		serverAddr.sin_family = AF_INET;
		InetPtonA(AF_INET, pIp, &serverAddr.sin_addr);
		serverAddr.sin_port = htons(port);

		if (!g_pLanClientManager->ConnectEx(pConnectSession->m_sessionSocket, (sockaddr *)&serverAddr, sizeof(serverAddr),
			&pConnectSession->m_overlappeds[0]))
		{
			errVal = WSAGetLastError();
			if (errVal != ERROR_IO_PENDING)
			{
				if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET)
					g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"AcceptEx() Error : %d", errVal);

				if (InterlockedDecrement(&pConnectSession->m_ioCountAndRelease) == 0)
					ReleaseSession(pConnectSession);
				return FALSE;
			}
		}

		return TRUE;
	}

	BOOL CLanClient::ConnectExCompleted(CLanSession *pSession) noexcept
	{
		int retVal;
		int errVal;

		// SO_UPDATE_CONNECT_CONTEXT 옵션 설정
		retVal = setsockopt(pSession->m_sessionSocket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"setsockopt(SO_UPDATE_CONNECT_CONTEXT) 실패 : %d", errVal);
			return FALSE;
		}

		return TRUE;
	}

	unsigned int LanWorkerThreadFunc(LPVOID pParam) noexcept
	{
		return g_pLanClientManager->WorkerThread();
	}

	BOOL CLanClientManager::Init() noexcept
	{
		int retVal;
		int errVal;
		WSAData wsaData;

		retVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (retVal != 0)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"WSAStartup() 실패 : %d", errVal);
			return FALSE;
		}

		m_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, clientSetting::iocpActiveThread);
		if (m_iocpHandle == NULL)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"CreateIoCompletionPort(생성) 실패 : %d", errVal);
			return FALSE;
		}

		// 더미 소켓
		SOCKET dummySocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
		if (dummySocket == INVALID_SOCKET)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"WSASocket() 실패 : %d", errVal);
			return FALSE;
		}

		DWORD dwBytes;
		retVal = WSAIoctl(dummySocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidConnectEx, sizeof(m_guidConnectEx),
			&m_pConnectEx, sizeof(m_pConnectEx), &dwBytes, NULL, NULL);
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"WSAIoctl(lpfnConnectEx) 실패 : %d", errVal);
			return FALSE;
		}

		closesocket(dummySocket);

		return TRUE;
	}

	BOOL CLanClientManager::Start() noexcept
	{
		int errVal;

		// CreateWorkerThread
		for (int i = 1; i <= clientSetting::iocpWorkerThread; i++)
		{
			HANDLE hWorkerThread = (HANDLE)_beginthreadex(nullptr, 0, LanWorkerThreadFunc, nullptr, 0, nullptr);
			if (hWorkerThread == 0)
			{
				errVal = GetLastError();
				g_pLogger->WriteLog(L"SYSTEM", L"NetworkLib", ELogLevel::Error, L"WorkerThread[%d] running fail.. : %d", i, errVal);
				return FALSE;
			}

			m_arrWorkerThreads.push_back(hWorkerThread);
			g_pLogger->WriteLogConsole(ELogLevel::System, L"[SYSTEM] WorkerThread[%d] running..", i);
		}

		return TRUE;
	}

	BOOL CLanClientManager::Wait() noexcept
	{
		WaitForMultipleObjects(static_cast<DWORD>(m_arrWorkerThreads.size()), m_arrWorkerThreads.data(), TRUE, INFINITE);
		return TRUE;
	}

	bool CLanClientManager::RegisterIoCompletionPort(SOCKET sock, ULONG_PTR sessionId) noexcept
	{
		return CreateIoCompletionPort(reinterpret_cast<HANDLE>(sock), m_iocpHandle, sessionId, 0) != nullptr;
	}

	int CLanClientManager::WorkerThread() noexcept
	{
		while (m_isWorkerRun)
		{
			DWORD transferred = 0;
			ULONG_PTR key = 0;
			OVERLAPPED *pOverlapped = nullptr;
			const BOOL isCompleted = GetQueuedCompletionStatus(m_iocpHandle, &transferred, &key, &pOverlapped, INFINITE);
			if (pOverlapped == nullptr)
			{
				if (isCompleted)
					PostQueuedCompletionStatus(m_iocpHandle, 0, 0, nullptr);
				break;
			}

			auto *pSession = reinterpret_cast<CLanSession *>(key);
			// 등록된 I/O 또는 종료 통지가 테이블에서 세션을 유지한다.
			auto spSession = pSession->shared_from_this();
			auto *pClient = m_arrNetClients[pSession->m_clientMgrIndex];
			if (pOverlapped == reinterpret_cast<OVERLAPPED *>(EIoOperation::ReleaseSession))
			{
				pClient->ReleaseSessionPqcs(pSession);
				continue;
			}
			if (pOverlapped == reinterpret_cast<OVERLAPPED *>(EIoOperation::SendPost))
			{
				pSession->PostSend();
				continue;
			}
			if (pOverlapped == &pSession->m_overlappeds[0])
			{
				if (isCompleted && !InterlockedCompareExchange(&pSession->m_cancelIoCalled, FALSE, FALSE)
					&& pClient->ConnectExCompleted(pSession))
				{
					pSession->m_isConnected = true;
					pClient->OnConnect(pSession->m_sessionId);
					if (!pSession->PostRecv())
						pClient->Disconnect(pSession->m_sessionId);
				}
			}
			else if (!isCompleted || transferred == 0)
			{
				pClient->Disconnect(pSession->m_sessionId);
			}
			else if (pOverlapped == &pSession->m_overlappeds[1])
			{
				pSession->RecvCompleted(transferred);
				if (!pSession->PostRecv())
					pClient->Disconnect(pSession->m_sessionId);
			}
			else if (pOverlapped == &pSession->m_overlappeds[2])
			{
				pSession->SendCompleted(transferred);
			}

			if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
				pClient->ReleaseSession(pSession);
		}
		return 0;
	}
}
