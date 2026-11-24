// PacketGenerator generated file. Do not edit.
#pragma once

namespace WorldServer
{
	struct SReqAdd;
	struct SReqChange;
	struct SReqMove;

	class IServerPacketHandler
	{
	public:
		virtual bool HandleReqAdd(UINT64 sessionId, const SReqAdd &packet) noexcept = 0;
		virtual bool HandleReqChange(UINT64 sessionId, const SReqChange &packet) noexcept = 0;
		virtual bool HandleReqMove(UINT64 sessionId, const SReqMove &packet) noexcept = 0;

	protected:
		virtual ~IServerPacketHandler() = default;
	};
}