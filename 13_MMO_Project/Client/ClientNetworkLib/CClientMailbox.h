#pragma once

namespace ClientNetworkLib
{
	class CNetSession;
}

namespace NetworkLib
{
	template<bool isLanServer> class CSerializableBuffer;

	class CClientMailbox
	{
	private:
		friend class CNetClientManager;
		friend class ClientNetworkLib::CNetSession;

		[[nodiscard]] bool tryPop(SClientEvent &outEvent) noexcept;
		[[nodiscard]] bool pushPacket(std::shared_ptr<CSerializableBuffer<FALSE>> spPacket) noexcept;
		void publishConnected() noexcept;
		void publishTerminalEvent(EClientEvent kind, DWORD error) noexcept;

		std::mutex m_mailboxLock;
		// 수명 이벤트는 큐 할당 실패나 패킷 적체와 관계없이 전달한다.
		bool m_hasConnected = false;
		std::optional<SClientEvent> m_terminal;
		std::deque<std::shared_ptr<CSerializableBuffer<FALSE>>> m_packets;
	};
}
