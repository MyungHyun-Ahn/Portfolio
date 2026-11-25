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

		// 모든 테이블을 읽은 뒤 필요한 파생 데이터를 준비한다.
		[[nodiscard]]
		virtual bool Init() = 0;

		// 모든 테이블을 초기화한 뒤 읽기 전용으로 규칙을 검사한다.
		[[nodiscard]]
		virtual bool Validate() const = 0;
	};
}
