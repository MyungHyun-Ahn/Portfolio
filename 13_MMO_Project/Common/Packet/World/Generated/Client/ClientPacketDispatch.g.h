// PacketGenerator generated file. Do not edit.
#pragma once

namespace WorldServer
{
	struct SResAdd;
	struct SResChange;
	struct SResMove;

	class IClientPacketHandler;

	[[nodiscard]]
	bool DispatchClientPacket(NetworkCommonLib::CSerializableBuffer &message, IClientPacketHandler &handler) noexcept;
}
