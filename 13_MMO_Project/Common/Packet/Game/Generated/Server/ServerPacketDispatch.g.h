// PacketGenerator generated file. Do not edit.
#pragma once

namespace Game
{
	struct SReqLogin;

	class IServerPacketHandler;

	[[nodiscard]]
	bool DispatchServerPacket(UINT64 sessionId, NetworkCommonLib::CSerializableBufferView &message,
		IServerPacketHandler &handler, std::uint16_t *outOpcode = nullptr) noexcept;
}
