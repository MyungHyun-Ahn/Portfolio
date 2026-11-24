// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	struct SReqSectorMove final : public MHLib::serialization::ISerializable
	{
		static constexpr EPacketType kPacketType = EPacketType::ReqSectorMove;

		std::int64_t m_accountNo{};
		std::uint16_t m_sectorX{};
		std::uint16_t m_sectorY{};

		[[nodiscard]] bool Serialize(NetworkCommonLib::CSerializableBuffer &buffer) const noexcept override
		{
			buffer
				<< m_accountNo
				<< m_sectorX
				<< m_sectorY;
			return buffer.IsValid();
		}

		[[nodiscard]] bool Deserialize(NetworkCommonLib::CSerializableBuffer &buffer) noexcept override
		{
			buffer
				>> m_accountNo
				>> m_sectorX
				>> m_sectorY;
			return buffer.IsValid();
		}

		// 수신 View에서 패킷 본문을 바로 읽는다.
		[[nodiscard]]
		bool Deserialize(NetworkCommonLib::CSerializableBufferView &view) noexcept
		{
			view
				>> m_accountNo
				>> m_sectorX
				>> m_sectorY;
			return view.IsValid();
		}
	};

	struct SResSectorMove final : public MHLib::serialization::ISerializable
	{
		static constexpr EPacketType kPacketType = EPacketType::ResSectorMove;

		std::int64_t m_accountNo{};
		std::uint16_t m_sectorX{};
		std::uint16_t m_sectorY{};

		[[nodiscard]] bool Serialize(NetworkCommonLib::CSerializableBuffer &buffer) const noexcept override
		{
			buffer
				<< m_accountNo
				<< m_sectorX
				<< m_sectorY;
			return buffer.IsValid();
		}

		[[nodiscard]] bool Deserialize(NetworkCommonLib::CSerializableBuffer &buffer) noexcept override
		{
			buffer
				>> m_accountNo
				>> m_sectorX
				>> m_sectorY;
			return buffer.IsValid();
		}

		// 수신 View에서 패킷 본문을 바로 읽는다.
		[[nodiscard]]
		bool Deserialize(NetworkCommonLib::CSerializableBufferView &view) noexcept
		{
			view
				>> m_accountNo
				>> m_sectorX
				>> m_sectorY;
			return view.IsValid();
		}
	};
}
