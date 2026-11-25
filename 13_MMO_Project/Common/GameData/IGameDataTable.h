#pragma once

namespace GameData
{
	class IGameDataTable
	{
	public:
		virtual ~IGameDataTable() = default;

		// 후보 파일을 읽고 이 테이블이 소유하는 행과 PK 조회를 준비한다.
		[[nodiscard]]
		virtual bool Load(const std::filesystem::path& path) = 0;

		// 다른 테이블이나 공개 상태에 의존하지 않고 로컬 규칙만 검사한다.
		[[nodiscard]]
		virtual bool Validate() const = 0;
	};
}
