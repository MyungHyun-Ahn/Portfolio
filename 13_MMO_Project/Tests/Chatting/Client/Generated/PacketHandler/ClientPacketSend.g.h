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
}
