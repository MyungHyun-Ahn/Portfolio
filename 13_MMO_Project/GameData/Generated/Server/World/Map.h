#pragma once

namespace GameData::Server::World
{
	struct SMapRow
	{
		std::uint32_t m_mapId{};
		::GameData::MapType m_mapType{};
		std::int32_t m_minXCm{};
		std::int32_t m_minYCm{};
		std::int32_t m_maxXCm{};
		std::int32_t m_maxYCm{};
		std::int32_t m_spawnXCm{};
		std::int32_t m_spawnYCm{};

		[[nodiscard]]
		std::uint32_t GetMapId() const noexcept
		{
			return m_mapId;
		}

		[[nodiscard]]
		::GameData::MapType GetMapType() const noexcept
		{
			return m_mapType;
		}

		[[nodiscard]]
		std::int32_t GetMinXCm() const noexcept
		{
			return m_minXCm;
		}

		[[nodiscard]]
		std::int32_t GetMinYCm() const noexcept
		{
			return m_minYCm;
		}

		[[nodiscard]]
		std::int32_t GetMaxXCm() const noexcept
		{
			return m_maxXCm;
		}

		[[nodiscard]]
		std::int32_t GetMaxYCm() const noexcept
		{
			return m_maxYCm;
		}

		[[nodiscard]]
		std::int32_t GetSpawnXCm() const noexcept
		{
			return m_spawnXCm;
		}

		[[nodiscard]]
		std::int32_t GetSpawnYCm() const noexcept
		{
			return m_spawnYCm;
		}
	};

	namespace MapFormat
	{
		inline constexpr std::uint16_t kFormatVersion = 1;
		inline constexpr std::uint8_t kTarget = 1;
		inline constexpr char kTableName[] = "Map";
		inline constexpr std::uint8_t kSchemaHash[32] =
		{
			0xd8, 0x2a, 0x4b, 0xa7, 0x0b, 0xd3, 0xb8, 0xaa, 0xfa, 0xfd, 0x78, 0x2c, 0x84, 0x76, 0xcb, 0xc1,
			0xbc, 0x8f, 0x21, 0xe7, 0x6f, 0xbb, 0xd4, 0x43, 0x9c, 0x51, 0xfa, 0x3d, 0xca, 0x1e, 0xec, 0x46,
		};
	}
}
