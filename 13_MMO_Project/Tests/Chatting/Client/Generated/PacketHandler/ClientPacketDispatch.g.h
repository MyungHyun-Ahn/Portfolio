// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	class IClientPacketHandler;

	[[nodiscard]] bool DispatchClientPacket(NetworkCommonLib::CSerializableBuffer &message, IClientPacketHandler &handler) noexcept;
}
