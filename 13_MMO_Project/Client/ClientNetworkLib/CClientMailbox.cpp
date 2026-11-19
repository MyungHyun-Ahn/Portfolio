#include "pch.h"
#include "CClientMailbox.h"

namespace NetworkLib
{
	bool CClientMailbox::TryPop(SClientEvent &outEvent) noexcept
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
		else if (m_hasTerminal)
		{
			outEvent = std::move(m_terminal);
			m_hasTerminal = false;
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

	void CClientMailbox::publishTerminal(EClientEvent kind, DWORD error) noexcept
	{
		std::lock_guard mailboxLock(m_mailboxLock);
		m_terminal = { kind, error, {} };
		m_hasTerminal = true;
	}
}
