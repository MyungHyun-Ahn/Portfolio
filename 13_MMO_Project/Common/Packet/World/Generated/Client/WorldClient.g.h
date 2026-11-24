// PacketGenerator generated file. Do not edit.
#pragma once

#include "IClientPacketHandler.g.h"

namespace WorldServer
{
	struct SReqAdd;

	[[nodiscard]] std::shared_ptr<NetworkCommonLib::CSerializableBuffer> MakePacketReqAdd(
		const SReqAdd &packet) noexcept;

	class WorldClient : public ClientNetworkLib::CClient, private IClientPacketHandler
	{
	public:
		[[nodiscard]] bool SendReqAdd(
			std::uint64_t mapId,
			std::uint32_t amount) noexcept;

	protected:
		using ClientNetworkLib::CClient::CClient;

	private:
		bool onRecv(NetworkCommonLib::CSerializableBuffer &message) noexcept override final;
		bool HandleResAdd(const SResAdd &packet) noexcept override final;

#include "WorldClientMembers.inc"
	};
}
