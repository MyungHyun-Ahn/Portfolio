// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	struct SReqMessage final : public MHLib::serialization::ISerializable
	{
		static constexpr EPacketType kPacketType = EPacketType::ReqMessage;

		std::int64_t m_accountNo{};
		std::wstring m_message{};

		[[nodiscard]] bool Serialize(NetworkCommonLib::CSerializableBuffer &buffer) const noexcept override
		{
			buffer
				<< m_accountNo
				<< m_message;
			return buffer.IsValid();
		}

		[[nodiscard]] bool Deserialize(NetworkCommonLib::CSerializableBuffer &buffer) noexcept override
		{
			buffer
				>> m_accountNo
				>> m_message;
			return buffer.IsValid();
		}
	};

	struct SResMessage final : public MHLib::serialization::ISerializable
	{
		static constexpr EPacketType kPacketType = EPacketType::ResMessage;

		std::int64_t m_accountNo{};
		std::wstring m_id{};
		std::wstring m_nickname{};
		std::wstring m_message{};

		[[nodiscard]] bool Serialize(NetworkCommonLib::CSerializableBuffer &buffer) const noexcept override
		{
			buffer
				<< m_accountNo
				<< m_id
				<< m_nickname
				<< m_message;
			return buffer.IsValid();
		}

		[[nodiscard]] bool Deserialize(NetworkCommonLib::CSerializableBuffer &buffer) noexcept override
		{
			buffer
				>> m_accountNo
				>> m_id
				>> m_nickname
				>> m_message;
			return buffer.IsValid();
		}
	};

	struct SMessageNoti final : public MHLib::serialization::ISerializable
	{
		static constexpr EPacketType kPacketType = EPacketType::MessageNoti;

		std::int64_t m_accountNo{};
		std::wstring m_id{};
		std::wstring m_nickname{};
		std::wstring m_message{};

		[[nodiscard]] bool Serialize(NetworkCommonLib::CSerializableBuffer &buffer) const noexcept override
		{
			buffer
				<< m_accountNo
				<< m_id
				<< m_nickname
				<< m_message;
			return buffer.IsValid();
		}

		[[nodiscard]] bool Deserialize(NetworkCommonLib::CSerializableBuffer &buffer) noexcept override
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
