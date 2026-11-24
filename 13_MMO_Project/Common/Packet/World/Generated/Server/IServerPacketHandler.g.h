// PacketGenerator generated file. Do not edit.
#pragma once

namespace WorldServer
{
	struct SReqAdd;

	class IServerPacketHandler
	{
	public:
		virtual bool HandleReqAdd(UINT64 sessionId, const SReqAdd &packet) noexcept = 0;

	protected:
		virtual ~IServerPacketHandler() = default;
	};
}