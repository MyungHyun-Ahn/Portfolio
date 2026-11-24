// PacketGenerator generated file. Do not edit.
#pragma once

namespace WorldServer
{
	struct SResAdd;
	struct SResChange;
	struct SResMove;

	class IClientPacketHandler
	{
	public:
		virtual bool HandleResAdd(const SResAdd &packet) noexcept = 0;
		virtual bool HandleResChange(const SResChange &packet) noexcept = 0;
		virtual bool HandleResMove(const SResMove &packet) noexcept = 0;

	protected:
		virtual ~IClientPacketHandler() = default;
	};
}