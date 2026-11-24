// PacketGenerator generated file. Do not edit.
#pragma once

namespace WorldServer
{
	struct SReqChange final : public MHLib::serialization::ISerializable
	{
		static constexpr EPacketType kPacketType = EPacketType::ReqChange;

		std::uint64_t m_mapId{};
		std::uint32_t m_amount{};

		[[nodiscard]] bool Serialize(NetworkCommonLib::CSerializableBuffer &buffer) const noexcept override
		{
			buffer
				<< m_mapId
				<< m_amount;
			return buffer.IsValid();
		}

		[[nodiscard]] bool Deserialize(NetworkCommonLib::CSerializableBuffer &buffer) noexcept override
		{
			buffer
				>> m_mapId
				>> m_amount;
			return buffer.IsValid();
		}

		// 수신 View에서 패킷 본문을 바로 읽는다.
		[[nodiscard]]
		bool Deserialize(NetworkCommonLib::CSerializableBufferView &view) noexcept
		{
			view
				>> m_mapId
				>> m_amount;
			return view.IsValid();
		}
	};

	struct SResChange final : public MHLib::serialization::ISerializable
	{
		static constexpr EPacketType kPacketType = EPacketType::ResChange;

		std::uint64_t m_mapId{};
		std::uint64_t m_characterValue{};
		std::uint16_t m_result{};

		[[nodiscard]] bool Serialize(NetworkCommonLib::CSerializableBuffer &buffer) const noexcept override
		{
			buffer
				<< m_mapId
				<< m_characterValue
				<< m_result;
			return buffer.IsValid();
		}

		[[nodiscard]] bool Deserialize(NetworkCommonLib::CSerializableBuffer &buffer) noexcept override
		{
			buffer
				>> m_mapId
				>> m_characterValue
				>> m_result;
			return buffer.IsValid();
		}

		// 수신 View에서 패킷 본문을 바로 읽는다.
		[[nodiscard]]
		bool Deserialize(NetworkCommonLib::CSerializableBufferView &view) noexcept
		{
			view
				>> m_mapId
				>> m_characterValue
				>> m_result;
			return view.IsValid();
		}
	};
}
