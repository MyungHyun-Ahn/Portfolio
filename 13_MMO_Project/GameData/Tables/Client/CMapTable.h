#pragma once

namespace GameData::Client::World
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

		// 프로젝트 레벨의 패키지 경로 형식을 확인한다.
		[[nodiscard]]
		bool Validate() const override
		{
			for (const auto& entry : GetRows())
			{
				const auto& path = entry.second.GetLevelAssetPath();
				if (!path.starts_with("/Game/") || path.back() == '/' || path.find("//") != path.npos ||
					path.find_first_of("\\.:*?\"<>|' ,&!~@#(){}[]=;^%$`") != path.npos)
				{
					return false;
				}
				for (const unsigned char ch : path)
				{
					if (ch < 32 || ch == 127)
					{
						return false;
					}
				}
			}
			return true;
		}
	};
}
