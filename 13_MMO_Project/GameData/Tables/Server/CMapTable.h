#pragma once

namespace GameData::Server::World
{
	class CMapTable final : public CMapTableBase
	{
	public:
		// 모든 테이블을 읽은 뒤 필요한 파생 데이터를 준비한다.
		[[nodiscard]]
		bool Init() override
		{
			return true;
		}

		// 모든 맵의 경계와 스폰 위치를 확인한다.
		[[nodiscard]]
		bool Validate() const override
		{
			for (const auto& entry : GetRows())
			{
				const auto& row = entry.second;
				if (row.GetMinXCm() >= row.GetMaxXCm() || row.GetMinYCm() >= row.GetMaxYCm() ||
					row.GetSpawnXCm() < row.GetMinXCm() || row.GetSpawnXCm() > row.GetMaxXCm() ||
					row.GetSpawnYCm() < row.GetMinYCm() || row.GetSpawnYCm() > row.GetMaxYCm())
				{
					return false;
				}
			}
			return true;
		}
	};
}
