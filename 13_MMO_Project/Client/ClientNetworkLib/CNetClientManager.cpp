#include "pch.h"
#include "CSerializableBuffer.h"
#include "CClientMailbox.h"
#include "CNetClientManager.h"
#include "CNetSession.h"

namespace NetworkLib
{
	using ClientNetworkLib::CNetSession;

	CNetClientManager::~CNetClientManager()
	{
		Stop();
	}

	// ClientManager 초기화
	bool CNetClientManager::Init(DWORD workerCount) noexcept
	{
		std::lock_guard lifecycleLock(m_lifecycleLock);
		if (workerCount == 0 || m_iocpHandle != nullptr)
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
				m_workers.emplace_back(&CNetClientManager::workerThread, this, workerIndex == 0);
		}
		catch (...)
		{
			stopWorkersLocked();
			return false;
		}
		std::lock_guard sessionTableLock(m_sessionTableLock);
		m_pConnectEx = pConnectEx;
		m_connectProtocol = connectProtocol;
		m_isAccepting = true;
		return true;
	}

	SessionId CNetClientManager::Connect(const char *pIp, USHORT port, BYTE packetCode,
		const std::shared_ptr<CClientMailbox> &spMailbox, DWORD timeoutMilliseconds) noexcept
	{
		SOCKADDR_IN address{};
		address.sin_family = AF_INET;
		address.sin_port = htons(port);
		if (spMailbox == nullptr || pIp == nullptr || port == 0 || InetPtonA(AF_INET, pIp, &address.sin_addr) != 1)
			return 0;

		// Stop이 포트와 Winsock을 정리하기 전에 등록과 ConnectEx 제출을 마친다.
		std::lock_guard sessionTableLock(m_sessionTableLock);
		if (!m_isAccepting || m_lastSessionId == (std::numeric_limits<SessionId>::max)() || spMailbox->m_isBound.load())
			return 0;
		try
		{
			auto spSession = std::make_shared<CNetSession>(packetCode, spMailbox);
			// ConnectEx를 조회했던 공급자로 세션 소켓을 생성한다.
			spSession->m_socket = WSASocketW(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO,
				&m_connectProtocol, 0, WSA_FLAG_OVERLAPPED);
			SOCKADDR_IN localAddress{};
			localAddress.sin_family = AF_INET;
			if (spSession->m_socket == INVALID_SOCKET
				|| bind(spSession->m_socket, reinterpret_cast<const sockaddr *>(&localAddress), sizeof(localAddress)) == SOCKET_ERROR)
				return 0;
			const SessionId sessionId = ++m_lastSessionId;
			if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(spSession->m_socket), m_iocpHandle, sessionId, 0) == nullptr)
				return 0;
			m_sessions.emplace(sessionId, spSession);
			if (spMailbox->m_isBound.exchange(true))
			{
				m_sessions.erase(sessionId);
				return 0;
			}
			// 워커는 테이블 락이 풀린 뒤에만 세션을 가져올 수 있다.
			if (!spSession->postConnectLocked(m_pConnectEx, address, timeoutMilliseconds))
			{
				spMailbox->m_isBound = false;
				m_sessions.erase(sessionId);
				return 0;
			}
			return sessionId;
		}
		catch (const std::bad_alloc &)
		{
			return 0;
		}
	}

	bool CNetClientManager::SendPacket(SessionId sessionId, std::shared_ptr<CSerializableBuffer<FALSE>> spMessage) noexcept
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

	bool CNetClientManager::Disconnect(SessionId sessionId, DWORD drainMilliseconds) noexcept
	{
		auto spSession = findSession(sessionId);
		if (spSession == nullptr)
			return false;
		bool isFinished;
		{
			std::unique_lock sessionStateLock(spSession->m_sessionStateLock);
			if (spSession->m_state == CNetSession::EState::Closing)
				return false;
			if (drainMilliseconds != 0 && spSession->m_state == CNetSession::EState::Connected && !spSession->m_isDrainingSend)
			{
				spSession->m_isDrainingSend = true;
				spSession->m_sendDrainedCv.wait_for(sessionStateLock, std::chrono::milliseconds(drainMilliseconds), [&spSession]
				{
					return spSession->m_spSending == nullptr || spSession->m_state == CNetSession::EState::Closing;
				});
			}
			spSession->beginCloseLocked(ERROR_OPERATION_ABORTED);
			isFinished = spSession->finishCloseLocked();
		}
		if (isFinished)
			retireSession(sessionId);
		return true;
	}

	std::shared_ptr<CNetSession> CNetClientManager::findSession(SessionId sessionId) noexcept
	{
		std::lock_guard sessionTableLock(m_sessionTableLock);
		auto found = m_sessions.find(sessionId);
		return found != m_sessions.end() ? found->second : nullptr;
	}

	bool CNetClientManager::copySessionSnapshot(std::vector<SSessionEntry> &outSessions, SessionId &outLastId) noexcept
	{
		std::lock_guard sessionTableLock(m_sessionTableLock);
		outLastId = m_lastSessionId;
		try
		{
			outSessions.reserve(m_sessions.size());
		}
		catch (const std::bad_alloc &)
		{
			return false;
		}
		for (const auto &entry : m_sessions)
			outSessions.emplace_back(entry.first, entry.second);
		return true;
	}

	CNetClientManager::SSessionEntry CNetClientManager::findNextSessionWithoutSnapshot(SessionId cursor, SessionId lastId) noexcept
	{
		std::lock_guard sessionTableLock(m_sessionTableLock);
		SSessionEntry next{};
		for (const auto &[sessionId, spSession] : m_sessions)
		{
			if (sessionId > cursor && sessionId <= lastId && (next.second == nullptr || sessionId < next.first))
				next = { sessionId, spSession };
		}
		return next;
	}

	void CNetClientManager::retireSession(SessionId sessionId) noexcept
	{
		std::lock_guard sessionTableLock(m_sessionTableLock);
		m_sessions.erase(sessionId);
		if (m_sessions.empty())
			m_sessionTableEmptyCv.notify_all();
	}

	void CNetClientManager::expireConnects() noexcept
	{
		const auto now = GetTickCount64();
		const auto expireSession = [now](const SSessionEntry &entry)
		{
			const auto &spSession = entry.second;
			std::lock_guard sessionStateLock(spSession->m_sessionStateLock);
			// Connecting 상태에는 아직 소비하지 않은 ConnectEx가 한 건 있다.
			if (spSession->m_state == CNetSession::EState::Connecting && now >= spSession->m_connectDeadline)
				spSession->beginCloseLocked(WSAETIMEDOUT);
		};
		std::vector<SSessionEntry> sessions;
		SessionId lastId;
		if (copySessionSnapshot(sessions, lastId))
		{
			for (const auto &entry : sessions)
				expireSession(entry);
			return;
		}
		// 메모리가 부족해도 타임아웃 처리를 건너뛰지 않는다.
		for (SessionId cursor = 0;;)
		{
			const auto entry = findNextSessionWithoutSnapshot(cursor, lastId);
			if (entry.second == nullptr)
				break;
			cursor = entry.first;
			expireSession(entry);
		}
	}

	void CNetClientManager::workerThread(bool isTimeoutWorker) noexcept
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

	void CNetClientManager::Stop() noexcept
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
			SessionId lastId;
			if (copySessionSnapshot(sessions, lastId))
			{
				for (const auto &entry : sessions)
					closeSessionForStop(entry);
			}
			else
			{
				// 스냅샷 할당이 실패해도 종료 요청을 모두 보낸다.
				for (SessionId cursor = 0;;)
				{
					const auto entry = findNextSessionWithoutSnapshot(cursor, lastId);
					if (entry.second == nullptr)
						break;
					cursor = entry.first;
					closeSessionForStop(entry);
				}
			}
		}
		{
			std::unique_lock sessionTableLock(m_sessionTableLock);
			m_sessionTableEmptyCv.wait(sessionTableLock, [this] { return m_sessions.empty(); });
		}
		stopWorkersLocked();
	}

	void CNetClientManager::closeSessionForStop(const SSessionEntry &entry) noexcept
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

	void CNetClientManager::stopWorkersLocked() noexcept
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
