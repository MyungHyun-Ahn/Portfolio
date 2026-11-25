#pragma once

namespace GameData::Server::Player
{
	struct SPlayerMovementRow
	{
		std::uint32_t m_playerMovementDataId{};
		std::uint32_t m_moveSpeedCmPerSecond{};
		std::uint32_t m_collisionRadiusCm{};

		[[nodiscard]]
		std::uint32_t GetPlayerMovementDataId() const noexcept
		{
			return m_playerMovementDataId;
		}

		[[nodiscard]]
		std::uint32_t GetMoveSpeedCmPerSecond() const noexcept
		{
			return m_moveSpeedCmPerSecond;
		}

		[[nodiscard]]
		std::uint32_t GetCollisionRadiusCm() const noexcept
		{
			return m_collisionRadiusCm;
		}
	};

	namespace PlayerMovementFormat
	{
		inline constexpr std::uint16_t kFormatVersion = 1;
		inline constexpr std::uint8_t kTarget = 1;
		inline constexpr char kTableName[] = "PlayerMovement";
		inline constexpr std::uint8_t kSchemaHash[32] =
		{
			0x09, 0xdf, 0xfb, 0xfc, 0xac, 0x67, 0x87, 0x45, 0xa4, 0xdc, 0xe9, 0x7f, 0xf8, 0x76, 0x9e, 0x63,
			0xf2, 0x0b, 0xd1, 0x0a, 0x91, 0x29, 0x8c, 0xba, 0xae, 0x4a, 0x0f, 0x86, 0xf2, 0xc7, 0xab, 0xaf,
		};
	}
}
