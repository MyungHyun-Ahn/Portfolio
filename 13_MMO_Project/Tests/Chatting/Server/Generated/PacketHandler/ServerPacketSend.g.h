// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	struct SResMessage;
	struct SResLogin;
	struct SResSectorMove;

	[[nodiscard]] NetworkLib::CSerializableBuffer<FALSE> *MakePacketResMessage(
		const SResMessage &packet) noexcept;
	[[nodiscard]] NetworkLib::CSerializableBuffer<FALSE> *MakePacketResLogin(
		const SResLogin &packet) noexcept;
	[[nodiscard]] NetworkLib::CSerializableBuffer<FALSE> *MakePacketResSectorMove(
		const SResSectorMove &packet) noexcept;
}
