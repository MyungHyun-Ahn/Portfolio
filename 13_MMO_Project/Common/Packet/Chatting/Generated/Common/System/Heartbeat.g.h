// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	struct SHeartbeatNoti final : public MHLib::serialization::ISerializable
	{
		static constexpr EPacketType kPacketType = EPacketType::HeartbeatNoti;

		std::int64_t m_accountNo{};

		[[nodiscard]] bool Serialize(NetworkCommonLib::CSerializableBuffer &buffer) const noexcept override
		{
			buffer
				<< m_accountNo;
			return buffer.IsValid();
		}

		[[nodiscard]] bool Deserialize(NetworkCommonLib::CSerializableBuffer &buffer) noexcept override
		{
			buffer
				>> m_accountNo;
			return buffer.IsValid();
		}

		// 수신 View에서 패킷 본문을 바로 읽는다.
		[[nodiscard]]
		bool Deserialize(NetworkCommonLib::CSerializableBufferView &view) noexcept
		{
			view
				>> m_accountNo;
			return view.IsValid();
		}
	};
}
