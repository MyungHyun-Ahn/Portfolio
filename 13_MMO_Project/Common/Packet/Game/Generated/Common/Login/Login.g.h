// PacketGenerator generated file. Do not edit.
#pragma once

namespace Game
{
	struct SReqLogin final : public MHLib::serialization::ISerializable
	{
		static constexpr EPacketType kPacketType = EPacketType::ReqLogin;

		std::wstring m_loginId{};

		[[nodiscard]] bool Serialize(NetworkCommonLib::CSerializableBuffer &buffer) const noexcept override
		{
			buffer
				<< m_loginId;
			return buffer.IsValid();
		}

		[[nodiscard]] bool Deserialize(NetworkCommonLib::CSerializableBuffer &buffer) noexcept override
		{
			buffer
				>> m_loginId;
			return buffer.IsValid();
		}

		// 수신 View에서 패킷 본문을 바로 읽는다.
		[[nodiscard]]
		bool Deserialize(NetworkCommonLib::CSerializableBufferView &view) noexcept
		{
			view
				>> m_loginId;
			return view.IsValid();
		}
	};

	struct SResLogin final : public MHLib::serialization::ISerializable
	{
		static constexpr EPacketType kPacketType = EPacketType::ResLogin;

		static_assert(static_cast<std::uint32_t>(::GameData::EResultCode::Success) == 0u);
		static_assert(static_cast<std::uint32_t>(::GameData::EResultCode::Rejected) == 1u);
		::GameData::EResultCode m_result{};
		SCharacterData m_characterData{};

		[[nodiscard]] bool Serialize(NetworkCommonLib::CSerializableBuffer &buffer) const noexcept override
		{
			if (m_result != ::GameData::EResultCode::Success &&
				m_result != ::GameData::EResultCode::Rejected)
			{
				return false;
			}
			buffer
				<< static_cast<std::uint8_t>(m_result)
				<< m_characterData;
			return buffer.IsValid();
		}

		[[nodiscard]] bool Deserialize(NetworkCommonLib::CSerializableBuffer &buffer) noexcept override
		{
			{
				std::uint8_t value{};
				buffer >> value;
				const auto decoded = static_cast<::GameData::EResultCode>(value);
				if (!buffer.IsValid() ||
					(decoded != ::GameData::EResultCode::Success &&
					 decoded != ::GameData::EResultCode::Rejected))
				{
					return false;
				}
				m_result = decoded;
			}
			buffer >> m_characterData;
			return buffer.IsValid();
		}

		// 수신 View에서 패킷 본문을 바로 읽는다.
		[[nodiscard]]
		bool Deserialize(NetworkCommonLib::CSerializableBufferView &view) noexcept
		{
			{
				std::uint8_t value{};
				view >> value;
				const auto decoded = static_cast<::GameData::EResultCode>(value);
				if (!view.IsValid() ||
					(decoded != ::GameData::EResultCode::Success &&
					 decoded != ::GameData::EResultCode::Rejected))
				{
					return false;
				}
				m_result = decoded;
			}
			if (!view.IsValid() || !m_characterData.Deserialize(view))
			{
				return false;
			}
			return view.IsValid();
		}
	};
}
