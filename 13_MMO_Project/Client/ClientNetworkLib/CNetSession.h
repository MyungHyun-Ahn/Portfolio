#pragma once

namespace ClientNetworkLib
{
	class CNetSession
	{
	public:
		CNetSession(BYTE packetCode, std::shared_ptr<NetworkLib::CClientMailbox> spMailbox);
		~CNetSession();

	private:
		friend class NetworkLib::CNetClientManager;
		friend class NetworkLib::CClientLifecycleProbe;

		enum class EState
		{
			Connecting,
			Connected,
			Closing
		};

		// 호출자는 매니저의 m_sessionTableLock을 잡아 세션 조회를 막은 상태에서 호출한다.
		[[nodiscard]] bool postConnectLocked(LPFN_CONNECTEX pConnectEx, const SOCKADDR_IN &address, DWORD timeoutMilliseconds) noexcept;

		// 아래 함수들은 호출자가 m_sessionStateLock을 잡은 상태에서만 호출한다.
		[[nodiscard]] bool queueSendLocked(std::shared_ptr<NetworkLib::CSerializableBuffer<FALSE>> spMessage) noexcept;
		[[nodiscard]] bool postSendLocked() noexcept;
		[[nodiscard]] bool postRecvLocked() noexcept;
		void completeLocked(OVERLAPPED *pOverlapped, DWORD bytes, DWORD error) noexcept;
		void connectCompletedLocked() noexcept;
		void sendCompletedLocked(DWORD bytes) noexcept;
		[[nodiscard]] DWORD recvCompletedLocked(DWORD bytes);
		void beginCloseLocked(DWORD error) noexcept;
		[[nodiscard]] bool finishCloseLocked() noexcept;

		// 세션의 상태를 동기화하기 위함
		std::mutex m_sessionStateLock;
		// 대기시간을 준 Disconnect에 사용
		std::condition_variable m_sendDrainedCv;
		SOCKET m_socket = INVALID_SOCKET;
		EState m_state = EState::Connecting;
		bool m_isDrainingSend = false;
		bool m_wasConnected = false;
		bool m_isFinalized = false;
		DWORD m_closeError = 0;
		ULONGLONG m_connectDeadline = 0;
		int m_pendingIo = 0;
		BYTE m_packetCode;
		std::shared_ptr<NetworkLib::CClientMailbox> m_spMailbox;
		OVERLAPPED m_connectOverlapped{};
		OVERLAPPED m_recvOverlapped{};
		OVERLAPPED m_sendOverlapped{};
		std::array<char, 4096> m_recvBytes{};
		std::vector<char> m_recvStream;
		std::deque<std::shared_ptr<NetworkLib::CSerializableBuffer<FALSE>>> m_sendQueue;
		std::shared_ptr<NetworkLib::CSerializableBuffer<FALSE>> m_spSending;
		DWORD m_sentBytes = 0;
	};
}
