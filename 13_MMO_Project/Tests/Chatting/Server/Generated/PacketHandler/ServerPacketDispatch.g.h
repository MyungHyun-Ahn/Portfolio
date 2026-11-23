// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	class IServerPacketHandler;

	[[nodiscard]] bool DispatchServerPacket(std::uint16_t type, UINT64 sessionId,
NetworkCommonLib::CSerializableBuffer &message, IServerPacketHandler &handler) noexcept;
}
