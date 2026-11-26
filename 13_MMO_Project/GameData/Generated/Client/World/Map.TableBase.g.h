#pragma once

namespace GameData::Client::World
{
	class CMapTableBase : public IGameDataTable
	{
	public:
		[[nodiscard]]
		bool Load(const std::filesystem::path& path) final
		{
			static_assert(MapFormat::kFormatVersion == CGameDataReader::kSupportedFormatVersion);
			const SGameDataFormat format
			{
				MapFormat::kFormatVersion,
				MapFormat::kTarget,
				MapFormat::kTableName,
				MapFormat::kSchemaHash,
			};
			m_lastReadResult = CGameDataReader::Read(path, format, &readRow, &SMapRow::m_mapId, m_rows);
			return m_lastReadResult == EGameDataReadResult::Success;
		}

		[[nodiscard]]
		EGameDataReadResult GetLastReadResult() const noexcept
		{
			return m_lastReadResult;
		}

		// 현재 묶음에서 PK에 해당하는 읽기 전용 행을 찾는다.
		[[nodiscard]]
		static const SMapRow* Find(std::uint32_t id);

	protected:
		[[nodiscard]]
		const auto& GetRows() const noexcept
		{
			return m_rows;
		}

	private:
		[[nodiscard]]
		static bool readRow(CGameDataReader& reader, SMapRow& outRow)
		{
			if (!reader.ReadValue(outRow.m_mapId))
			{
				return false;
			}
			if (!reader.ReadValue(outRow.m_levelAssetPath))
			{
				return false;
			}
			return true;
		}

	private:
		std::unordered_map<std::uint32_t, SMapRow> m_rows;
		EGameDataReadResult m_lastReadResult = EGameDataReadResult::NotRead;
	};
}
