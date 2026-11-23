#pragma once

namespace ClientNetworkLib
{
	class CNetSession;
}

namespace NetworkLib
{
	template<bool isLanServer> class CSerializableBuffer;
	class CClientMailbox;
	struct SClientEvent;
	class CClientLifecycleProbe;
	using SessionId = ULONG_PTR;

	class CNetClientManager
	{
	public:
		CNetClientManager() = default;
		~CNetClientManager();
		CNetClientManager(const CNetClientManager &) = delete;
		CNetClientManager &operator=(const CNetClientManager &) = delete;

		[[nodiscard]]
		bool Init(DWORD workerCount, int recvBufferBytes) noexcept;

		// 0은 Connect 예약에 실패함.
		// - 0이 아닌 값은 SessionId이다.
		[[nodiscard]]
		SessionId Connect(const char *pIp, USHORT port, BYTE packetCode, DWORD timeoutMilliseconds) noexcept;

		// Stop 이후에도 읽을 수 있다. 종료 이벤트를 읽으면 해당 ID의 Mailbox가 제거된다.
		[[nodiscard]]
		bool TryPopEvent(SessionId sessionId, SClientEvent &outEvent) noexcept;

		// MakeShared()으로 만든 새 패킷을 한 번만 넘긴다. 송신 접수 후에는 내용이나 커서를 변경하지 않는다.
		[[nodiscard]]
		bool SendPacket(SessionId sessionId, std::shared_ptr<CSerializableBuffer<FALSE>> spMessage) noexcept;
		bool Disconnect(SessionId sessionId) noexcept;
		void Stop() noexcept;

	private:
		// 수명 테스트 전용 헬퍼 클래스
		// - ConnectEx 성공 여부 검사, Stop 시 세션 잘 끊겼는지를 검사하기 위해
		// - 13_MMO_Project\Tests\ClientLifecycle 프로젝트에 정의하고 사용함
		friend class CClientLifecycleProbe;

		using SSessionEntry = std::pair<SessionId, std::shared_ptr<ClientNetworkLib::CNetSession>>;

		[[nodiscard]]
		std::shared_ptr<ClientNetworkLib::CNetSession> findSession(SessionId sessionId) noexcept;
		// 전체 순회 시 테이블 락을 걸고 id와 session 스마트 포인터의 쌍을 얻는 함수
		// - 스마트 포인터로 얻기 때문에 순회 중 객체가 삭제되지 않는 것을 보장한다. (연결은 끊길 수 있다.)
		// - 테이블을 얻기만 하기 때문에 전체 테이블을 Lock 잡는 시간이 적다.
		void copySessionSnapshot(std::vector<SSessionEntry> &outSessions) noexcept;

		void closeSessionForStop(const SSessionEntry &entry) noexcept;
		void retireSession(SessionId sessionId) noexcept;

		// 접속 타임아웃 검사 함수
		void expireConnects() noexcept;

		void workerThread(bool isTimeoutWorker) noexcept;
		// 호출자는 m_lifecycleLock을 잡아야 한다.
		void stopWorkersLocked() noexcept;

	private:
		std::mutex 				m_lifecycleLock; // Init과 Stop을 동기화 하기 위한 목적
		// 테이블 락을 잡은 채 세션 락을 기다리지 않는다. 송신에서는 세션 → 테이블 순서로 잠근다.
		// 두 테이블을 보호한다. Mailbox는 테이블 또는 세션 락 안에서 잠글 수 있지만 역순으로 잠그지 않는다.
		std::mutex 				m_sessionTableLock;
		// Stop에서 마지막 세션이 테이블에서 제거되는 것을 기다리는 용도
		std::condition_variable m_sessionTableEmptyCv;

		// Mailbox와 세션은 수명주기가 다르다.
		// - Disconnected를 처리하기 위해 Mailbox가 길 수 있음
		std::unordered_map<SessionId, std::shared_ptr<ClientNetworkLib::CNetSession>> 	m_sessions;
		std::unordered_map<SessionId, std::shared_ptr<CClientMailbox>> 					m_mailboxes;

		// 세션 ID 발급 용도 - 단순 증가
		SessionId m_lastSessionId = 0;

		// Init 시 켜지고 Stop 시 꺼지는 플래그
		// - ClientManager가 서버로 Connect와 Send 요청을 보낼 수 있는 상태를 나타냄
		bool 	m_isAccepting = false;
		int 	m_recvBufferBytes = 0;

		HANDLE 				m_iocpHandle = nullptr;
		LPFN_CONNECTEX 		m_pConnectEx = nullptr;
		WSAPROTOCOL_INFOW 	m_connectProtocol{};

		std::atomic<bool> 	m_areWorkersStopping = false;
		std::vector<std::thread> m_workers;
	};
}
