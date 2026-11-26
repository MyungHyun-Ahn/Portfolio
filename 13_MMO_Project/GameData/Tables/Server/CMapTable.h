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

		// 테이블 규칙을 구현하기 전에는 사용하지 않는다.
		[[nodiscard]]
		bool Validate() const override
		{
			return false;
		}
	};
}
