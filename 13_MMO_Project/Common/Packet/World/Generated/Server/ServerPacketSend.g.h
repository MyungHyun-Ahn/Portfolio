// PacketGenerator generated file. Do not edit.
#pragma once

namespace WorldServer
{
	struct SResAdd;
	struct SResChange;
	struct SResMove;

	template<typename TPacket>
		requires (
			std::same_as<TPacket, SResAdd> ||
			std::same_as<TPacket, SResChange> ||
			std::same_as<TPacket, SResMove>)
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

		bool SendResAdd(
			UINT64 sessionId,
			std::uint64_t mapId,
			std::uint64_t total,
			std::uint16_t result) noexcept;
		bool SendResChange(
			UINT64 sessionId,
			std::uint64_t mapId,
			std::uint64_t characterValue,
			std::uint16_t result) noexcept;
		bool SendResMove(
			UINT64 sessionId,
			std::uint64_t mapId,
			std::uint64_t characterValue,
			std::uint16_t result) noexcept;

	protected:
		virtual ~CServerPacketSender() = default;

	private:
		virtual void sendPacket(UINT64 sessionId, NetworkCommonLib::CSerializableBuffer &packet) noexcept = 0;
	};
}
