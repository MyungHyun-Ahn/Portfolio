// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	struct SReqMessage;
	struct SReqLogin;
	struct SReqSectorMove;
	struct SHeartbeatNoti;

	[[nodiscard]] std::shared_ptr<NetworkLib::CSerializableBuffer<FALSE>> MakePacketReqMessage(
		const SReqMessage &packet) noexcept;
	[[nodiscard]] std::shared_ptr<NetworkLib::CSerializableBuffer<FALSE>> MakePacketReqLogin(
		const SReqLogin &packet) noexcept;
	[[nodiscard]] std::shared_ptr<NetworkLib::CSerializableBuffer<FALSE>> MakePacketReqSectorMove(
		const SReqSectorMove &packet) noexcept;
	[[nodiscard]] std::shared_ptr<NetworkLib::CSerializableBuffer<FALSE>> MakePacketHeartbeatNoti(
		const SHeartbeatNoti &packet) noexcept;

	class CChatNetClient : public NetworkLib::CNetClient, private IClientPacketHandler
	{
	public:
		[[nodiscard]] bool SendReqMessage(

			std::int64_t accountNo,
			const std::wstring &message) noexcept;
		[[nodiscard]] bool SendReqLogin(

			std::int64_t accountNo,
			const std::array<WCHAR, 20> &id,
			const std::array<WCHAR, 20> &nickname,
			const std::array<char, 64> &sessionKey) noexcept;
		[[nodiscard]] bool SendReqSectorMove(

			std::int64_t accountNo,
			std::uint16_t sectorX,
			std::uint16_t sectorY) noexcept;
		[[nodiscard]] bool SendNotiHeartbeat(

			std::int64_t accountNo) noexcept;

	protected:
		using NetworkLib::CNetClient::CNetClient;

	private:
		bool onRecv(NetworkLib::CSerializableBuffer<FALSE> &message) noexcept override final;
	};
}
