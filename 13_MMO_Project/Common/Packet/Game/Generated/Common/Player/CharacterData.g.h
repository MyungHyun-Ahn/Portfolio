// PacketGenerator generated file. Do not edit.
#pragma once

namespace Game
{
	struct SCharacterData final : public MHLib::serialization::ISerializable
	{
		std::uint64_t m_characterId{};
		std::uint32_t m_lastLogoutMap{};

		[[nodiscard]]
		bool Serialize(NetworkCommonLib::CSerializableBuffer &buffer) const noexcept override
		{
			buffer
				<< m_characterId
				<< m_lastLogoutMap;
			return buffer.IsValid();
		}

		[[nodiscard]]
		bool Deserialize(NetworkCommonLib::CSerializableBuffer &buffer) noexcept override
		{
			buffer
				>> m_characterId
				>> m_lastLogoutMap;
			return buffer.IsValid();
		}

		// 수신 View에서 패킷 본문을 바로 읽는다.
		[[nodiscard]]
		bool Deserialize(NetworkCommonLib::CSerializableBufferView &view) noexcept
		{
			view
				>> m_characterId
				>> m_lastLogoutMap;
			return view.IsValid();
		}
	};
}
