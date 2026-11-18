// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	struct SReqMessage;
	struct SReqLogin;
	struct SReqSectorMove;
	struct SHeartbeatNoti;

	[[nodiscard]] NetworkLib::CSerializableBuffer<FALSE> *MakePacketReqMessage(
		const SReqMessage &packet) noexcept;
	[[nodiscard]] NetworkLib::CSerializableBuffer<FALSE> *MakePacketReqLogin(
		const SReqLogin &packet) noexcept;
	[[nodiscard]] NetworkLib::CSerializableBuffer<FALSE> *MakePacketReqSectorMove(
		const SReqSectorMove &packet) noexcept;
	[[nodiscard]] NetworkLib::CSerializableBuffer<FALSE> *MakePacketHeartbeatNoti(
		const SHeartbeatNoti &packet) noexcept;
}
