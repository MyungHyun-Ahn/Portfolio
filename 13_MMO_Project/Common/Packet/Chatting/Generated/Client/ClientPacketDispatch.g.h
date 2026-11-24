// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	struct SResMessage;
	struct SMessageNoti;
	struct SResLogin;
	struct SResSectorMove;

	class IClientPacketHandler;

	[[nodiscard]]
	bool DispatchClientPacket(NetworkCommonLib::CSerializableBuffer &message, IClientPacketHandler &handler) noexcept;
}
