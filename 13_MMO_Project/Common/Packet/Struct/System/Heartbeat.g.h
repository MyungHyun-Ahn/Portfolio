// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	struct SHeartbeatNoti final : public MHLib::serialization::ISerializable<false>
	{
		std::int64_t m_accountNo{};

		[[nodiscard]] bool Serialize(NetworkCommonLib::CSerializableBuffer<false> &buffer) const noexcept override
		{
			buffer
				<< m_accountNo;
			return buffer.IsValid();
		}

		[[nodiscard]] bool Deserialize(NetworkCommonLib::CSerializableBuffer<false> &buffer) noexcept override
		{
			buffer
				>> m_accountNo;
			return buffer.IsValid();
		}
	};
}
