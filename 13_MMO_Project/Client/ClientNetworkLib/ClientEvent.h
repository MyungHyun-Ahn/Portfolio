#pragma once

namespace ClientNetworkLib
{
	enum class EClientEvent
	{
		Connected,
		Packet,
		ConnectFailed,
		Disconnected
	};

	struct SClientEvent
	{
		EClientEvent 									m_kind{};
		DWORD 											m_error = 0;
		std::shared_ptr<NetworkCommonLib::CSerializableBuffer<FALSE>> 	m_spPacket;
	};
}
