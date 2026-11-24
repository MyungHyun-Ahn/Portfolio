// PacketGenerator generated file. Do not edit.
#pragma once

namespace WorldServer
{
	struct SResAdd;

	class IClientPacketHandler
	{
	public:
		virtual bool HandleResAdd(const SResAdd &packet) noexcept = 0;

	protected:
		virtual ~IClientPacketHandler() = default;
	};
}