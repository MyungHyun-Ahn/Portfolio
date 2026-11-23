#include "ClientNetworkLibPch.h"

namespace ClientNetworkLib
{
	using namespace NetworkCommonLib;

	CClient::CClient(CClientManager &network) noexcept
		: m_network(network)
	{
	}

	bool CClient::Connect(const char *pIp, USHORT port, BYTE packetCode, DWORD timeoutMilliseconds) noexcept
	{
		if (IsActive())
			return false;

		m_sessionId = m_network.Connect(pIp, port, packetCode, timeoutMilliseconds);
		return IsActive();
	}

	void CClient::Disconnect() noexcept
	{
		m_isConnected = false;
		if (IsActive())
			m_network.Disconnect(m_sessionId);
	}

	// 클라이언트는 Tick을 가지고 매 프레임마다 이 함수를 호출한다.
	void CClient::Tick() noexcept
	{
		if (!IsActive())
			return;

		// 매 Tick() 호출마다 kMaxEventsPerTick의 개수만큼 메일박스 이벤트를 꺼내서 처리한다.
		constexpr int kMaxEventsPerTick = 64;
		SClientEvent event;
		for (int count = 0; count < kMaxEventsPerTick && m_network.TryPopEvent(m_sessionId, event); ++count)
		{
			switch (event.m_kind)
			{
			case EClientEvent::Connected:
			{
				m_isConnected = true;
				if (!onConnect())
					Disconnect();

				break;
			}
			case EClientEvent::Packet:
			{
				if (m_isConnected && !onRecv(*event.m_spPacket))
				{
					onRecvRejected();
					Disconnect();
				}

				break;
			}
			case EClientEvent::ConnectFailed:
			case EClientEvent::Disconnected:
			{
				m_sessionId = 0;
				m_isConnected = false;
				onDisconnect(event.m_kind == EClientEvent::Disconnected, event.m_error);

				break;
			}
			}
		}
	}

	bool CClient::IsActive() const noexcept
	{
		return m_sessionId != 0;
	}

	bool CClient::IsConnected() const noexcept
	{
		return m_isConnected;
	}

	bool CClient::sendPacket(std::shared_ptr<CSerializableBuffer> spPacket) noexcept
	{
		return m_network.SendPacket(m_sessionId, std::move(spPacket));
	}
}