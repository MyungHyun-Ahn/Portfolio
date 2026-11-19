#pragma once

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
}
