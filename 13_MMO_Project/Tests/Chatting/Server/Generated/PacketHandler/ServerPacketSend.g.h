// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	struct SResMessage;
	struct SMessageNoti;
	struct SResLogin;
	struct SResSectorMove;

	template<typename TPacket>
		requires (
			std::same_as<TPacket, SResMessage> ||
			std::same_as<TPacket, SMessageNoti> ||
			std::same_as<TPacket, SResLogin> ||
			std::same_as<TPacket, SResSectorMove>)
	[[nodiscard]]
	ServerNetworkLib::SendPacketPtr MakePacket(const TPacket &packet) noexcept
	{
		auto spBuffer = ServerNetworkLib::SendPacketPtr::MakeShared();
		*spBuffer << static_cast<std::uint16_t>(TPacket::kPacketType) << packet;
		if (!spBuffer->IsValid())
			return {};
		return spBuffer;
	}

	class CServerPacketSender
	{
	public:
		// 버퍼가 있으면 송신을 요청한다. 접수나 전송 완료는 보장하지 않는다.
		bool SendPacket(UINT64 sessionId, const ServerNetworkLib::SendPacketPtr &spPacket) noexcept;

		bool SendResMessage(

			UINT64 sessionId,
			std::int64_t accountNo,
			const std::wstring &id,
			const std::wstring &nickname,
			const std::wstring &message) noexcept;
		bool SendNotiMessage(

			UINT64 sessionId,
			std::int64_t accountNo,
			const std::wstring &id,
			const std::wstring &nickname,
			const std::wstring &message) noexcept;
		bool SendResLogin(

			UINT64 sessionId,
			std::uint8_t status,
			std::int64_t accountNo) noexcept;
		bool SendResSectorMove(

			UINT64 sessionId,
			std::int64_t accountNo,
			std::uint16_t sectorX,
			std::uint16_t sectorY) noexcept;

	protected:
		virtual ~CServerPacketSender() = default;

	private:
		virtual void sendPacket(UINT64 sessionId, NetworkCommonLib::CSerializableBuffer &packet) noexcept = 0;
	};
}
