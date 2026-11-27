// PacketGenerator generated file. Do not edit.
#pragma once

namespace Game
{
	struct SReqLogin;

	class IServerPacketHandler
	{
	public:
		virtual bool HandleReqLogin(UINT64 sessionId, const SReqLogin &packet) noexcept = 0;

	protected:
		virtual ~IServerPacketHandler() = default;
	};
}