#pragma once

namespace ClientNetworkLib
{
	class CNetSession
	{
	public:
		CNetSession(BYTE packetCode, std::shared_ptr<CClientMailbox> spMailbox, int recvBufferBytes);
		~CNetSession();

	private:
		friend class CNetClientManager;
		friend class CClientLifecycleProbe;

		enum class EState
		{
			Connecting,
			Connected,
			Closing
		};

		// 호출자는 매니저의 m_sessionTableLock을 잡아 세션 조회를 막은 상태에서 호출한다.
		[[nodiscard]]
		bool postConnectLocked(LPFN_CONNECTEX pConnectEx, const SOCKADDR_IN &address, DWORD timeoutMilliseconds) noexcept;

		// 아래 함수들은 호출자가 m_sessionStateLock을 잡은 상태에서만 호출한다.
		[[nodiscard]]
		bool queueSendLocked(std::shared_ptr<NetworkCommonLib::CSerializableBuffer<FALSE>> spMessage) noexcept;
		[[nodiscard]]
		bool postSendLocked() noexcept;
		[[nodiscard]]
		bool postRecvLocked() noexcept;

		void completeLocked(OVERLAPPED *pOverlapped, DWORD bytes, DWORD error) noexcept;
		void connectCompletedLocked() noexcept;
		void sendCompletedLocked() noexcept;
		[[nodiscard]]
		DWORD recvCompletedLocked(DWORD bytes) noexcept;

		// Send 할 때 걸었던 Buffer의 참조를 해제한다.
		void releaseSendBatchLocked() noexcept;
		void beginCloseLocked(DWORD error) noexcept;
		[[nodiscard]]
		bool finishCloseLocked() noexcept;

	private:
		// 세션의 상태를 동기화하기 위함
		std::mutex 	m_sessionStateLock;
		SOCKET 		m_socket = INVALID_SOCKET;
		EState 		m_state = EState::Connecting;
		bool 		m_wasConnected = false;	//
		bool 		m_isFinalized = false;		// 종료 이벤트가 2번 이상 발생되는 걸 방지하는 플래그

		DWORD 		m_closeError = 0;
		ULONGLONG 	m_connectDeadline = 0; // 연결 대기 만료 시간
		int 		m_ioCount = 0;

		// 헤더 식별 값
		BYTE m_packetCode;

		std::shared_ptr<CClientMailbox> m_spMailbox;

		OVERLAPPED m_connectOverlapped{};
		OVERLAPPED m_recvOverlapped{};
		OVERLAPPED m_sendOverlapped{};

		// Recv 관련
		NetworkCommonLib::CRingBuffer m_recvBuffer;

		// Send 관련
		std::deque<std::shared_ptr<NetworkCommonLib::CSerializableBuffer<FALSE>>> 									m_sendQueue;
		std::array<std::shared_ptr<NetworkCommonLib::CSerializableBuffer<FALSE>>, NetworkCommonLib::kWsasendMaxBufferCount> m_sendBatch;
		DWORD m_sendCount = 0;
	};
}
