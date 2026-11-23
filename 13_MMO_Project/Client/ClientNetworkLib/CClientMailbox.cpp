#include "ClientNetworkLibPch.h"

namespace ClientNetworkLib
{
	using namespace NetworkCommonLib;

	bool CClientMailbox::tryPop(SClientEvent &outEvent) noexcept
	{
		std::lock_guard mailboxLock(m_mailboxLock);
		if (m_hasConnected)
		{
			outEvent = { EClientEvent::Connected, 0, {} };
			m_hasConnected = false;
		}
		else if (!m_packets.empty())
		{
			outEvent = { EClientEvent::Packet, 0, std::move(m_packets.front()) };
			m_packets.pop_front();
		}
		else if (m_terminal)
		{
			outEvent = std::move(*m_terminal);
			m_terminal.reset();
		}
		else
			return false;
		return true;
	}

	bool CClientMailbox::pushPacket(std::shared_ptr<CSerializableBuffer<FALSE>> spPacket) noexcept
	{
		std::lock_guard mailboxLock(m_mailboxLock);
		constexpr size_t kMaxQueuedPackets = 512;
		if (m_packets.size() >= kMaxQueuedPackets)
			return false;
		try
		{
			m_packets.push_back(std::move(spPacket));
			return true;
		}
		catch (const std::bad_alloc &)
		{
			return false;
		}
	}

	void CClientMailbox::publishConnected() noexcept
	{
		std::lock_guard mailboxLock(m_mailboxLock);
		m_hasConnected = true;
	}

	// 종료 이벤트를 발행한다.
	// - Disconnected와 연결 전 종료시에 사용
	// - Mailbox에 종료 이벤트를 저장하고
	// - Tick()에서 이 이벤트를 꺼내면 onDisconnect가 호출된다.
	void CClientMailbox::publishTerminalEvent(EClientEvent kind, DWORD error) noexcept
	{
		std::lock_guard mailboxLock(m_mailboxLock);
		m_terminal = SClientEvent{ kind, error, {} };
	}
}
