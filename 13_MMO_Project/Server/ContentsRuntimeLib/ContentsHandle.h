#pragma once

namespace ContentsRuntimeLib
{
	class CContentsRuntime;
	struct SCompletionMailbox;

	// 전역 등록 락과 ID 조회 없이, 메일박스의 락만 잡고 완료 콜백을 예약하기 위한 핸들
	// - 콘텐츠와 워커의 수명은 연장하지 않는다.
	class ContentsHandle final
	{
	private:
		friend class CContentsRuntime;

		std::weak_ptr<SCompletionMailbox> m_wpMailbox;
	};
}
