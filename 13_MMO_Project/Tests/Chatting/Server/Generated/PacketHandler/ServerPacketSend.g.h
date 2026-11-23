// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	enum class EPacketType : std::uint16_t;
	struct SResMessage;
	struct SMessageNoti;
	struct SResLogin;
	struct SResSectorMove;

	[[nodiscard]] NetworkCommonLib::CSerializableBuffer *MakePacketResMessage(
		const SResMessage &packet) noexcept;
	[[nodiscard]] NetworkCommonLib::CSerializableBuffer *MakePacketMessageNoti(
		const SMessageNoti &packet) noexcept;
	[[nodiscard]] NetworkCommonLib::CSerializableBuffer *MakePacketResLogin(
		const SResLogin &packet) noexcept;
	[[nodiscard]] NetworkCommonLib::CSerializableBuffer *MakePacketResSectorMove(
		const SResSectorMove &packet) noexcept;

	class CServerPacketSender
	{
	public:
		// 준비 실패 시 nullptr. 반환한 참조 1개는 호출자가 반환한다. 첫 전송 성공을 뜻하지 않는다.
		[[nodiscard]] NetworkCommonLib::CSerializableBuffer *SendResMessage(

			UINT64 sessionId,
			std::int64_t accountNo,
			const std::wstring &id,
			const std::wstring &nickname,
			const std::wstring &message) noexcept;
		// 준비 실패 시 nullptr. 반환한 참조 1개는 호출자가 반환한다. 첫 전송 성공을 뜻하지 않는다.
		[[nodiscard]] NetworkCommonLib::CSerializableBuffer *SendNotiMessage(

			UINT64 sessionId,
			std::int64_t accountNo,
			const std::wstring &id,
			const std::wstring &nickname,
			const std::wstring &message) noexcept;
		// 준비 실패 시 nullptr. 반환한 참조 1개는 호출자가 반환한다. 첫 전송 성공을 뜻하지 않는다.
		[[nodiscard]] NetworkCommonLib::CSerializableBuffer *SendResLogin(

			UINT64 sessionId,
			std::uint8_t status,
			std::int64_t accountNo) noexcept;
		// 준비 실패 시 nullptr. 반환한 참조 1개는 호출자가 반환한다. 첫 전송 성공을 뜻하지 않는다.
		[[nodiscard]] NetworkCommonLib::CSerializableBuffer *SendResSectorMove(

			UINT64 sessionId,
			std::int64_t accountNo,
			std::uint16_t sectorX,
			std::uint16_t sectorY) noexcept;

	protected:
		virtual ~CServerPacketSender() = default;
		// 직렬화된 버퍼를 받아 호출자 참조를 확보한 뒤 전송한다.
		[[nodiscard]] NetworkCommonLib::CSerializableBuffer *submitPreparedPacket(
			UINT64 sessionId, EPacketType type, NetworkCommonLib::CSerializableBuffer *pPacket) noexcept;

	private:
		virtual void sendPacket(UINT64 sessionId, EPacketType type, NetworkCommonLib::CSerializableBuffer &packet) noexcept = 0;
	};
}
