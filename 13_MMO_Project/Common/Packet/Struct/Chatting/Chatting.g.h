// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	struct SReqMessage final : public MHLib::serialization::ISerializable<false>
	{
		std::int64_t m_accountNo{};
		std::wstring m_message{};

		[[nodiscard]] bool Serialize(NetworkLib::CSerializableBuffer<false> &buffer) const noexcept override
		{
			buffer
				<< m_accountNo
				<< m_message;
			return buffer.IsValid();
		}

		[[nodiscard]] bool Deserialize(NetworkLib::CSerializableBuffer<false> &buffer) noexcept override
		{
			buffer
				>> m_accountNo
				>> m_message;
			return buffer.IsValid();
		}
	};

	struct SResMessage final : public MHLib::serialization::ISerializable<false>
	{
		std::int64_t m_accountNo{};
		std::array<WCHAR, 20> m_id{};
		std::array<WCHAR, 20> m_nickname{};
		std::wstring m_message{};

		[[nodiscard]] bool Serialize(NetworkLib::CSerializableBuffer<false> &buffer) const noexcept override
		{
			buffer
				<< m_accountNo
				<< m_id
				<< m_nickname
				<< m_message;
			return buffer.IsValid();
		}

		[[nodiscard]] bool Deserialize(NetworkLib::CSerializableBuffer<false> &buffer) noexcept override
		{
			buffer
				>> m_accountNo
				>> m_id
				>> m_nickname
				>> m_message;
			return buffer.IsValid();
		}
	};
}
