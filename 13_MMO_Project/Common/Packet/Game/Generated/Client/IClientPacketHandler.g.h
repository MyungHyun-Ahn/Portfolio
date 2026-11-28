// PacketGenerator generated file. Do not edit.
#pragma once

namespace Game
{
	struct SResLogin;

	class IClientPacketHandler
	{
	public:
		virtual bool HandleResLogin(const SResLogin &packet) noexcept = 0;

	protected:
		virtual ~IClientPacketHandler() = default;
	};
}