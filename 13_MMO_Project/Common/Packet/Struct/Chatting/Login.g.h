// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	struct SReqLogin final : public MHLib::serialization::ISerializable
	{
		std::int64_t m_accountNo{};
		std::wstring m_id{};
		std::wstring m_nickname{};
		std::array<char, 64> m_sessionKey{};

		[[nodiscard]] bool Serialize(NetworkCommonLib::CSerializableBuffer &buffer) const noexcept override
		{
			buffer
				<< m_accountNo
				<< m_id
				<< m_nickname
				<< m_sessionKey;
			return buffer.IsValid();
		}

		[[nodiscard]] bool Deserialize(NetworkCommonLib::CSerializableBuffer &buffer) noexcept override
		{
			buffer
				>> m_accountNo
				>> m_id
				>> m_nickname
				>> m_sessionKey;
			return buffer.IsValid();
		}
	};

	struct SResLogin final : public MHLib::serialization::ISerializable
	{
		std::uint8_t m_status{};
		std::int64_t m_accountNo{};

		[[nodiscard]] bool Serialize(NetworkCommonLib::CSerializableBuffer &buffer) const noexcept override
		{
			buffer
				<< m_status
				<< m_accountNo;
			return buffer.IsValid();
		}

		[[nodiscard]] bool Deserialize(NetworkCommonLib::CSerializableBuffer &buffer) noexcept override
		{
			buffer
				>> m_status
				>> m_accountNo;
			return buffer.IsValid();
		}
	};
}
