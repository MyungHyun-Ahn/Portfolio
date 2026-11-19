#pragma once

namespace ClientNetworkLib
{
	class CNetSession;
}

namespace NetworkLib
{
	template<bool isLanServer> class CSerializableBuffer;

	enum class EClientEvent
	{
		Connected,
		Packet,
		ConnectFailed,
		Disconnected
	};

	struct SClientEvent
	{
		EClientEvent m_kind{};
		DWORD m_error = 0;
		std::shared_ptr<CSerializableBuffer<FALSE>> m_spPacket;
	};

	class CClientMailbox
	{
	public:
		[[nodiscard]] bool TryPop(SClientEvent &outEvent) noexcept;

	private:
		friend class CNetClientManager;
		friend class ClientNetworkLib::CNetSession;

		[[nodiscard]] bool pushPacket(std::shared_ptr<CSerializableBuffer<FALSE>> spPacket) noexcept;
		void publishConnected() noexcept;
		void publishTerminal(EClientEvent kind, DWORD error) noexcept;

		std::atomic<bool> m_isBound = false;
		std::mutex m_mailboxLock;
		// 수명 이벤트는 큐 할당 실패나 패킷 적체와 관계없이 전달한다.
		bool m_hasConnected = false;
		bool m_hasTerminal = false;
		SClientEvent m_terminal;
		std::deque<std::shared_ptr<CSerializableBuffer<FALSE>>> m_packets;
	};
}
