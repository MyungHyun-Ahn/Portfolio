// PacketGenerator generated file. Do not edit.
#pragma once

namespace Game
{
	struct SResLogin;

	class IClientPacketHandler;

	[[nodiscard]]
	bool DispatchClientPacket(NetworkCommonLib::CSerializableBuffer &message, IClientPacketHandler &handler) noexcept;
}
