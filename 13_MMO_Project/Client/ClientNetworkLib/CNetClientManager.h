#pragma once

namespace ClientNetworkLib
{
	class CNetSession;
}

namespace NetworkLib
{
	template<bool isLanServer> class CSerializableBuffer;
	class CClientMailbox;
	class CClientLifecycleProbe;
	using SessionId = ULONG_PTR;

	class CNetClientManager
	{
	public:
		CNetClientManager() = default;
		~CNetClientManager();
		CNetClientManager(const CNetClientManager &) = delete;
		CNetClientManager &operator=(const CNetClientManager &) = delete;

		[[nodiscard]] bool Init(DWORD workerCount) noexcept;
		// 0은 등록 실패. 그 외에는 세션 Mailbox에서 접속 결과를 받는다.
		[[nodiscard]] SessionId Connect(const char *pIp, USHORT port, BYTE packetCode,
			const std::shared_ptr<CClientMailbox> &spMailbox, DWORD timeoutMilliseconds = 5000) noexcept;
		// MakeShared()으로 만든 새 패킷을 한 번만 넘긴다. 송신 접수 후에는 내용이나 커서를 변경하지 않는다.
		[[nodiscard]] bool SendPacket(SessionId sessionId, std::shared_ptr<CSerializableBuffer<FALSE>> spMessage) noexcept;
		bool Disconnect(SessionId sessionId, DWORD drainMilliseconds = 0) noexcept;
		void Stop() noexcept;

	private:
		// 수명 테스트 전용 헬퍼 클래스
		// - ConnectEx 성공 여부 검사, Stop 시 세션 잘 끊겼는지를 검사하기 위해
		// - 13_MMO_Project\Tests\ClientLifecycle 프로젝트에 정의하고 사용함
		friend class CClientLifecycleProbe;

		using SSessionEntry = std::pair<SessionId, std::shared_ptr<ClientNetworkLib::CNetSession>>;

		[[nodiscard]] std::shared_ptr<ClientNetworkLib::CNetSession> findSession(SessionId sessionId) noexcept;
		[[nodiscard]] bool copySessionSnapshot(std::vector<SSessionEntry> &outSessions, SessionId &outLastId) noexcept;
		[[nodiscard]] SSessionEntry findNextSessionWithoutSnapshot(SessionId cursor, SessionId lastId) noexcept;
		void closeSessionForStop(const SSessionEntry &entry) noexcept;
		void retireSession(SessionId sessionId) noexcept;
		void expireConnects() noexcept;
		void workerThread(bool isTimeoutWorker) noexcept;
		// 호출자는 m_lifecycleLock을 잡아야 한다.
		void stopWorkersLocked() noexcept;

		std::mutex m_lifecycleLock; // Init과 Stop을 동기화 하기 위한 목적
		// 테이블 락을 잡은 채 세션 락을 기다리지 않는다. 송신에서는 세션 → 테이블 순서로 잠근다.
		std::mutex m_sessionTableLock; // 세션 테이블의 접근을 동기화하는 용도
		std::condition_variable m_sessionTableEmptyCv; // Stop에서 마지막 세션이 테이블에서 제거되는 것을 기다리는 용도
		std::unordered_map<SessionId, std::shared_ptr<ClientNetworkLib::CNetSession>> m_sessions;
		SessionId m_lastSessionId = 0;
		bool m_isAccepting = false;
		LPFN_CONNECTEX m_pConnectEx = nullptr;
		WSAPROTOCOL_INFOW m_connectProtocol{};
		std::atomic<bool> m_areWorkersStopping = false;
		HANDLE m_iocpHandle = nullptr;
		std::vector<std::thread> m_workers;
	};
}
