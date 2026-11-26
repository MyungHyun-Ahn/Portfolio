#pragma once

namespace GameData::Client::World
{
	struct SMapRow
	{
		std::uint32_t m_mapId{};
		std::string m_levelAssetPath{};

		[[nodiscard]]
		std::uint32_t GetMapId() const noexcept
		{
			return m_mapId;
		}

		[[nodiscard]]
		const std::string& GetLevelAssetPath() const noexcept
		{
			return m_levelAssetPath;
		}
	};

	namespace MapFormat
	{
		inline constexpr std::uint16_t kFormatVersion = 1;
		inline constexpr std::uint8_t kTarget = 2;
		inline constexpr char kTableName[] = "Map";
		inline constexpr std::uint8_t kSchemaHash[32] =
		{
			0xee, 0x1d, 0xca, 0x14, 0x00, 0x6c, 0xd3, 0xe0, 0x73, 0xcf, 0x72, 0xef, 0x4c, 0x84, 0x8b, 0xed,
			0x0b, 0xd2, 0x47, 0x32, 0x2e, 0x97, 0x46, 0xc3, 0x94, 0xf7, 0xb9, 0xb8, 0xc1, 0x35, 0x39, 0x06,
		};
	}
}
