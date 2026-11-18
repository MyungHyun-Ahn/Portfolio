// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	struct SHeartbeatNoti final : public MHLib::serialization::ISerializable<false>
	{
		std::int64_t m_accountNo{};

		[[nodiscard]] bool Serialize(NetworkLib::CSerializableBuffer<false> &buffer) const noexcept override
		{
			buffer
				<< m_accountNo;
			return buffer.IsValid();
		}

		[[nodiscard]] bool Deserialize(NetworkLib::CSerializableBuffer<false> &buffer) noexcept override
		{
			buffer
				>> m_accountNo;
			return buffer.IsValid();
		}
	};
}
