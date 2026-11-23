// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	struct SReqMessage;
	struct SReqLogin;
	struct SReqSectorMove;
	struct SHeartbeatNoti;

	[[nodiscard]] std::shared_ptr<NetworkCommonLib::CSerializableBuffer> MakePacketReqMessage(
		const SReqMessage &packet) noexcept;
	[[nodiscard]] std::shared_ptr<NetworkCommonLib::CSerializableBuffer> MakePacketReqLogin(
		const SReqLogin &packet) noexcept;
	[[nodiscard]] std::shared_ptr<NetworkCommonLib::CSerializableBuffer> MakePacketReqSectorMove(
		const SReqSectorMove &packet) noexcept;
	[[nodiscard]] std::shared_ptr<NetworkCommonLib::CSerializableBuffer> MakePacketHeartbeatNoti(
		const SHeartbeatNoti &packet) noexcept;

	class CChatNetClient : public ClientNetworkLib::CClient, private IClientPacketHandler
	{
	public:
		[[nodiscard]] bool SendReqMessage(

			std::int64_t accountNo,
			const std::wstring &message) noexcept;
		[[nodiscard]] bool SendReqLogin(

			std::int64_t accountNo,
			const std::wstring &id,
			const std::wstring &nickname,
			const std::array<char, 64> &sessionKey) noexcept;
		[[nodiscard]] bool SendReqSectorMove(

			std::int64_t accountNo,
			std::uint16_t sectorX,
			std::uint16_t sectorY) noexcept;
		[[nodiscard]] bool SendNotiHeartbeat(

			std::int64_t accountNo) noexcept;

	protected:
		using ClientNetworkLib::CClient::CClient;

	private:
		bool onRecv(NetworkCommonLib::CSerializableBuffer &message) noexcept override final;
		bool HandleResMessage(const SResMessage &packet) noexcept override final;
		bool HandleMessageNoti(const SMessageNoti &packet) noexcept override final;
		bool HandleResLogin(const SResLogin &packet) noexcept override final;
		bool HandleResSectorMove(const SResSectorMove &packet) noexcept override final;

#include "CChatNetClientMembers.inc"
	};
}
