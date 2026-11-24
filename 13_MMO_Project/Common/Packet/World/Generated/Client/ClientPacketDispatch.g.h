// PacketGenerator generated file. Do not edit.
#pragma once

namespace WorldServer
{
	struct SResAdd;

	class IClientPacketHandler;

	[[nodiscard]]
	bool DispatchClientPacket(NetworkCommonLib::CSerializableBuffer &message, IClientPacketHandler &handler) noexcept;
}
