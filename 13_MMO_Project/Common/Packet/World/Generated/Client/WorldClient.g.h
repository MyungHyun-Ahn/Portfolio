// PacketGenerator generated file. Do not edit.
#pragma once

#include "IClientPacketHandler.g.h"

namespace WorldServer
{
	struct SReqAdd;
	struct SReqChange;
	struct SReqMove;

	[[nodiscard]] std::shared_ptr<NetworkCommonLib::CSerializableBuffer> MakePacketReqAdd(
		const SReqAdd &packet) noexcept;
	[[nodiscard]] std::shared_ptr<NetworkCommonLib::CSerializableBuffer> MakePacketReqChange(
		const SReqChange &packet) noexcept;
	[[nodiscard]] std::shared_ptr<NetworkCommonLib::CSerializableBuffer> MakePacketReqMove(
		const SReqMove &packet) noexcept;

	class WorldClient : public ClientNetworkLib::CClient, private IClientPacketHandler
	{
	public:
		[[nodiscard]] bool SendReqAdd(
			std::uint64_t mapId,
			std::uint32_t amount) noexcept;
		[[nodiscard]] bool SendReqChange(
			std::uint64_t mapId,
			std::uint32_t amount) noexcept;
		[[nodiscard]] bool SendReqMove(
			std::uint64_t targetMapId) noexcept;

	protected:
		using ClientNetworkLib::CClient::CClient;

	private:
		bool onRecv(NetworkCommonLib::CSerializableBuffer &message) noexcept override final;
		bool HandleResAdd(const SResAdd &packet) noexcept override final;
		bool HandleResChange(const SResChange &packet) noexcept override final;
		bool HandleResMove(const SResMove &packet) noexcept override final;

#include "WorldClientMembers.inc"
	};
}
