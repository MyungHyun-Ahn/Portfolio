// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	struct SResMessage;
	struct SMessageNoti;
	struct SResLogin;
	struct SResSectorMove;

	class IClientPacketHandler
	{
	public:
		virtual ~IClientPacketHandler() = default;
		virtual bool HandleResMessage(const SResMessage &packet) noexcept = 0;
		virtual bool HandleMessageNoti(const SMessageNoti &packet) noexcept = 0;
		virtual bool HandleResLogin(const SResLogin &packet) noexcept = 0;
		virtual bool HandleResSectorMove(const SResSectorMove &packet) noexcept = 0;
	};
}
