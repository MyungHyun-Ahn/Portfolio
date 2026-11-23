#include "ClientNetworkLibPch.h"

namespace ClientNetworkLib
{
	using namespace NetworkCommonLib;

	CClientManager::~CClientManager()
	{
		Stop();
	}

	// ClientManager 초기화
	bool CClientManager::Init(DWORD workerCount, int recvBufferBytes, EPacketMode packetMode) noexcept
	{
		std::lock_guard lifecycleLock(m_lifecycleLock);
		constexpr int kMinRecvBufferBytes = static_cast<int>(CSerializableBuffer::EBufferDefine::PacketMaxSize);
		if (workerCount == 0 || m_iocpHandle != nullptr || recvBufferBytes < kMinRecvBufferBytes
			|| recvBufferBytes > kMaxRecvBufferBytes || (packetMode != EPacketMode::Lan && packetMode != EPacketMode::Net))
			return false;

		WSADATA wsaData{};
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
			return false;

		// Winsock 공급자 정보와 ConnectEx 함수를 얻어오기 위한 더미 소켓
		const SOCKET probeSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
		if (probeSocket == INVALID_SOCKET)
		{
			WSACleanup();
			return false;
		}

		WSAPROTOCOL_INFOW connectProtocol{};
		int protocolBytes = sizeof(connectProtocol);
		GUID connectExId = WSAID_CONNECTEX;
		LPFN_CONNECTEX pConnectEx = nullptr;
		DWORD returnedBytes = 0;
		const bool hasConnectEx = getsockopt(probeSocket, SOL_SOCKET, SO_PROTOCOL_INFOW,
			reinterpret_cast<char *>(&connectProtocol), &protocolBytes) == 0
			&& WSAIoctl(probeSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &connectExId, sizeof(connectExId),
				&pConnectEx, sizeof(pConnectEx), &returnedBytes, nullptr, nullptr) == 0
			&& pConnectEx != nullptr;
		closesocket(probeSocket);

		if (!hasConnectEx)
		{
			WSACleanup();
			return false;
		}

		m_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, workerCount);
		if (m_iocpHandle == nullptr)
		{
			WSACleanup();
			return false;
		}

		m_areWorkersStopping = false;

		try
		{
			m_workers.reserve(workerCount);
			for (DWORD workerIndex = 0; workerIndex < workerCount; ++workerIndex)
				m_workers.emplace_back(&CClientManager::workerThread, this, workerIndex == 0);
		}
		catch (...)
		{
			stopWorkersLocked();
			return false;
		}

		std::lock_guard sessionTableLock(m_sessionTableLock);
		m_pConnectEx = pConnectEx;
		m_connectProtocol = connectProtocol;
		m_recvBufferBytes = recvBufferBytes;
		m_packetMode = packetMode;
		m_isAccepting = true;
		return true;
	}

	SessionId CClientManager::Connect(const char *pIp, USHORT port, BYTE packetCode, DWORD timeoutMilliseconds) noexcept
	{
		SOCKADDR_IN address{};
		address.sin_family = AF_INET;
		address.sin_port = htons(port);
		if (pIp == nullptr || port == 0 || InetPtonA(AF_INET, pIp, &address.sin_addr) != 1)
			return 0;

		// Stop이 포트와 Winsock을 정리하기 전에 등록과 ConnectEx 제출을 마친다.
		std::lock_guard sessionTableLock(m_sessionTableLock);
		if (!m_isAccepting || m_lastSessionId == (std::numeric_limits<SessionId>::max)())
			return 0;

		SessionId sessionId = 0;
		try
		{
			auto spMailbox = std::make_shared<CClientMailbox>();
			auto spSession = std::make_shared<CSession>(packetCode, spMailbox, m_recvBufferBytes, m_packetMode);
			// ConnectEx를 조회했던 공급자로 세션 소켓을 생성한다.
			spSession->m_socket = WSASocketW(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO,
				&m_connectProtocol, 0, WSA_FLAG_OVERLAPPED);
			SOCKADDR_IN localAddress{};
			localAddress.sin_family = AF_INET;
			if (spSession->m_socket == INVALID_SOCKET
				|| bind(spSession->m_socket, reinterpret_cast<const sockaddr *>(&localAddress), sizeof(localAddress)) == SOCKET_ERROR)
				return 0;

			sessionId = ++m_lastSessionId;
			if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(spSession->m_socket), m_iocpHandle, sessionId, 0) == nullptr)
				return 0;

			m_sessions.emplace(sessionId, spSession);
			m_mailboxes.emplace(sessionId, std::move(spMailbox));
			// 두 항목을 등록한 뒤 제출한다. 워커는 테이블 락이 풀린 뒤에만 세션을 가져올 수 있다.
			if (spSession->postConnectLocked(m_pConnectEx, address, timeoutMilliseconds))
				return sessionId;
		}
		catch (const std::bad_alloc &)
		{
			// 두 테이블 중 한쪽만 등록된 경우도 아래에서 되돌린다.
		}

		m_sessions.erase(sessionId);
		m_mailboxes.erase(sessionId);
		return 0;
	}

	bool CClientManager::TryPopEvent(SessionId sessionId, SClientEvent &outEvent) noexcept
	{
		std::lock_guard sessionTableLock(m_sessionTableLock);
		const auto found = m_mailboxes.find(sessionId);
		if (found == m_mailboxes.end() || !found->second->tryPop(outEvent))
			return false;

		if (outEvent.m_kind == EClientEvent::ConnectFailed || outEvent.m_kind == EClientEvent::Disconnected)
			m_mailboxes.erase(found);
		return true;
	}

	bool CClientManager::SendPacket(SessionId sessionId, std::shared_ptr<CSerializableBuffer> spMessage) noexcept
	{
		if (spMessage == nullptr || !spMessage->IsHeapAllocated())
			return false;

		auto spSession = findSession(sessionId);
		if (spSession == nullptr)
			return false;

		bool isSent;
		bool isFinished;
		{
			std::lock_guard sessionStateLock(spSession->m_sessionStateLock);
			{
				// Stop의 접수 차단과 큐 등록·첫 송신 제출을 직렬화한다.
				std::lock_guard sessionTableLock(m_sessionTableLock);
				if (!m_isAccepting)
					return false;

				isSent = spSession->queueSendLocked(std::move(spMessage));
			}
			isFinished = spSession->finishCloseLocked();
		}
		if (isFinished)
			retireSession(sessionId);

		return isSent;
	}

	bool CClientManager::Disconnect(SessionId sessionId) noexcept
	{
		auto spSession = findSession(sessionId);
		if (spSession == nullptr)
			return false;

		bool isFinished;
		{
			std::lock_guard sessionStateLock(spSession->m_sessionStateLock);
			if (spSession->m_state == CSession::EState::Closing)
				return false;

			spSession->beginCloseLocked(ERROR_OPERATION_ABORTED);
			isFinished = spSession->finishCloseLocked();
		}

		if (isFinished)
			retireSession(sessionId);

		return true;
	}

	std::shared_ptr<CSession> CClientManager::findSession(SessionId sessionId) noexcept
	{
		std::lock_guard sessionTableLock(m_sessionTableLock);
		auto found = m_sessions.find(sessionId);
		return found != m_sessions.end() ? found->second : nullptr;
	}

	void CClientManager::copySessionSnapshot(std::vector<SSessionEntry> &outSessions) noexcept
	{
		std::lock_guard sessionTableLock(m_sessionTableLock);
		// 스냅샷 할당에 실패하면 순회를 건너뛰지 않고 프로세스를 종료한다.
		outSessions.reserve(m_sessions.size());
		for (const auto &entry : m_sessions)
			outSessions.emplace_back(entry.first, entry.second);
	}

	void CClientManager::retireSession(SessionId sessionId) noexcept
	{
		std::lock_guard sessionTableLock(m_sessionTableLock);
		m_sessions.erase(sessionId);
		if (m_sessions.empty())
			m_sessionTableEmptyCv.notify_all();
	}

	void CClientManager::expireConnects() noexcept
	{
		const auto now = GetTickCount64();
		const auto expireSession = [now](const SSessionEntry &entry)
		{
			const auto &spSession = entry.second;
			std::lock_guard sessionStateLock(spSession->m_sessionStateLock);
			// Connecting 상태에는 아직 소비하지 않은 ConnectEx가 한 건 있다.
			if (spSession->m_state == CSession::EState::Connecting && now >= spSession->m_connectDeadline)
				spSession->beginCloseLocked(WSAETIMEDOUT);
		};
		std::vector<SSessionEntry> sessions;
		copySessionSnapshot(sessions);

		for (const auto &entry : sessions)
			expireSession(entry);
	}

	void CClientManager::workerThread(bool isTimeoutWorker) noexcept
	{
		constexpr DWORD kTimeoutPollMilliseconds = 50;
		auto nextPoll = GetTickCount64() + kTimeoutPollMilliseconds;
		while (!m_areWorkersStopping)
		{
			DWORD bytes = 0;
			ULONG_PTR key = 0;
			OVERLAPPED *pOverlapped = nullptr;
			const BOOL isCompleted = GetQueuedCompletionStatus(m_iocpHandle, &bytes, &key, &pOverlapped,
				isTimeoutWorker ? kTimeoutPollMilliseconds : 1000);
			if (pOverlapped != nullptr)
			{
				const DWORD error = isCompleted ? 0 : GetLastError();
				auto spSession = findSession(key);
				if (spSession != nullptr)
				{
					bool isFinished;
					{
						std::lock_guard sessionStateLock(spSession->m_sessionStateLock);
						spSession->completeLocked(pOverlapped, bytes, error);
						isFinished = spSession->finishCloseLocked();
					}

					if (isFinished)
						retireSession(key);
				}
			}

			if (isTimeoutWorker && GetTickCount64() >= nextPoll)
			{
				expireConnects();
				nextPoll = GetTickCount64() + kTimeoutPollMilliseconds;
			}
		}
	}

	void CClientManager::Stop() noexcept
	{
		std::lock_guard lifecycleLock(m_lifecycleLock);
		if (m_iocpHandle == nullptr)
			return;
		{
			std::lock_guard sessionTableLock(m_sessionTableLock);
			m_isAccepting = false;
			m_pConnectEx = nullptr;
			m_connectProtocol = {};
		}
		{
			std::vector<SSessionEntry> sessions;
			copySessionSnapshot(sessions);
			for (const auto &entry : sessions)
				closeSessionForStop(entry);
		}
		{
			std::unique_lock sessionTableLock(m_sessionTableLock);
			m_sessionTableEmptyCv.wait(sessionTableLock, [this] { return m_sessions.empty(); });
		}
		stopWorkersLocked();
	}

	void CClientManager::closeSessionForStop(const SSessionEntry &entry) noexcept
	{
		const auto &spSession = entry.second;
		bool isFinished;
		{
			std::lock_guard sessionStateLock(spSession->m_sessionStateLock);
			spSession->beginCloseLocked(ERROR_OPERATION_ABORTED);
			isFinished = spSession->finishCloseLocked();
		}

		if (isFinished)
			retireSession(entry.first);
	}

	void CClientManager::stopWorkersLocked() noexcept
	{
		m_areWorkersStopping = true;
		for (size_t index = 0; index < m_workers.size(); ++index)
			PostQueuedCompletionStatus(m_iocpHandle, 0, 0, nullptr);

		for (auto &worker : m_workers)
			worker.join();

		m_workers.clear();
		CloseHandle(m_iocpHandle);
		m_iocpHandle = nullptr;
		WSACleanup();
	}
}
