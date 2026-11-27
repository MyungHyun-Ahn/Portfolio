// PacketGenerator generated file. Do not edit.
#pragma once

namespace GameData
{
	enum class EResultCode : std::uint32_t;
}

namespace Game
{
	struct SCharacterData;
	struct SResLogin;

	template<typename TPacket>
		requires (
			std::same_as<TPacket, SResLogin>)
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

		bool SendResLogin(
			UINT64 sessionId,
			::GameData::EResultCode result,
			const SCharacterData &characterData) noexcept;

	protected:
		virtual ~CServerPacketSender() = default;

	private:
		virtual void sendPacket(UINT64 sessionId, NetworkCommonLib::CSerializableBuffer &packet) noexcept = 0;
	};
}
