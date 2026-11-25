#pragma once

namespace GameData::Client
{
	struct SPlayerMovementRow
	{
		std::uint32_t m_playerMovementDataId{};
		std::uint32_t m_visualScalePermille{};

		[[nodiscard]]
		std::uint32_t GetPlayerMovementDataId() const noexcept
		{
			return m_playerMovementDataId;
		}

		[[nodiscard]]
		std::uint32_t GetVisualScalePermille() const noexcept
		{
			return m_visualScalePermille;
		}
	};

	namespace PlayerMovementFormat
	{
		inline constexpr std::uint16_t kFormatVersion = 1;
		inline constexpr std::uint8_t kTarget = 2;
		inline constexpr char kTableName[] = "PlayerMovement";
		inline constexpr std::uint8_t kSchemaHash[32] =
		{
			0xdf, 0xdc, 0xb3, 0x70, 0x67, 0xcd, 0x6d, 0x1a, 0x00, 0xb9, 0xc9, 0xa5, 0x86, 0x31, 0xac, 0x03,
			0xed, 0xc6, 0xb9, 0x7f, 0x77, 0x00, 0x32, 0xd6, 0x46, 0x84, 0xb8, 0x4f, 0x22, 0x0f, 0xbb, 0xdc,
		};
	}
}
