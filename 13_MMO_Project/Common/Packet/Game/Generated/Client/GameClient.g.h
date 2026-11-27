// PacketGenerator generated file. Do not edit.
#pragma once

#include "IClientPacketHandler.g.h"

namespace Game
{
	struct SReqLogin;

	[[nodiscard]] std::shared_ptr<NetworkCommonLib::CSerializableBuffer> MakePacketReqLogin(
		const SReqLogin &packet) noexcept;

	class GameClient : public ClientNetworkLib::CClient, private IClientPacketHandler
	{
	public:
		[[nodiscard]] bool SendReqLogin(
			const std::wstring &loginId) noexcept;

	protected:
		using ClientNetworkLib::CClient::CClient;

	private:
		bool onRecv(NetworkCommonLib::CSerializableBuffer &message) noexcept override final;
		bool HandleResLogin(const SResLogin &packet) noexcept override final;

#include "GameClientMembers.inc"
	};
}
