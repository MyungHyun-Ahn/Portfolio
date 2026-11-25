#pragma once

namespace GameData::Server::Player
{
	class CPlayerMovementTable final : public CPlayerMovementTableBase
	{
	public:
		// 파생 데이터가 필요하지 않아 바로 초기화를 마친다.
		[[nodiscard]]
		bool Init() override
		{
			return true;
		}

		// 필수 이동 프로필 테이블에 행이 있는지 확인한다.
		[[nodiscard]]
		bool Validate() const override
		{
			return !GetRows().empty();
		}
	};
}
