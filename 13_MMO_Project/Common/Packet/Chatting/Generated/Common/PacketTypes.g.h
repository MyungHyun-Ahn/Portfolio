// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	enum class EPacketType : std::uint16_t
	{
		ChatServer = 0,
		ReqLogin = 1,
		ResLogin = 2,
		ReqSectorMove = 3,
		ResSectorMove = 4,
		ReqMessage = 5,
		ResMessage = 6,
		HeartbeatNoti = 7,
		MessageNoti = 8,
	};
}
