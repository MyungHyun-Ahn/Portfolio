// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	struct SReqMessage;
	struct SReqLogin;
	struct SReqSectorMove;
	struct SHeartbeatNoti;

	class IServerPacketHandler
	{
	public:
		virtual ~IServerPacketHandler() = default;
		virtual bool HandleReqMessage(UINT64 sessionId, const SReqMessage &packet) noexcept = 0;
		virtual bool HandleReqLogin(UINT64 sessionId, const SReqLogin &packet) noexcept = 0;
		virtual bool HandleReqSectorMove(UINT64 sessionId, const SReqSectorMove &packet) noexcept = 0;
		virtual bool HandleHeartbeatNoti(UINT64 sessionId, const SHeartbeatNoti &packet) noexcept = 0;
	};
}
