// PacketGenerator generated file. Do not edit.
#pragma once

namespace Chatting
{
	struct SReqSectorMove final : public MHLib::serialization::ISerializable<false>
	{
		std::int64_t m_accountNo{};
		std::uint16_t m_sectorX{};
		std::uint16_t m_sectorY{};

		[[nodiscard]] bool Serialize(NetworkCommonLib::CSerializableBuffer<false> &buffer) const noexcept override
		{
			buffer
				<< m_accountNo
				<< m_sectorX
				<< m_sectorY;
			return buffer.IsValid();
		}

		[[nodiscard]] bool Deserialize(NetworkCommonLib::CSerializableBuffer<false> &buffer) noexcept override
		{
			buffer
				>> m_accountNo
				>> m_sectorX
				>> m_sectorY;
			return buffer.IsValid();
		}
	};

	struct SResSectorMove final : public MHLib::serialization::ISerializable<false>
	{
		std::int64_t m_accountNo{};
		std::uint16_t m_sectorX{};
		std::uint16_t m_sectorY{};

		[[nodiscard]] bool Serialize(NetworkCommonLib::CSerializableBuffer<false> &buffer) const noexcept override
		{
			buffer
				<< m_accountNo
				<< m_sectorX
				<< m_sectorY;
			return buffer.IsValid();
		}

		[[nodiscard]] bool Deserialize(NetworkCommonLib::CSerializableBuffer<false> &buffer) noexcept override
		{
			buffer
				>> m_accountNo
				>> m_sectorX
				>> m_sectorY;
			return buffer.IsValid();
		}
	};
}
