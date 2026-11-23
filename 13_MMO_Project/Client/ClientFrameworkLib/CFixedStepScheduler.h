#pragma once

namespace ClientFrameworkLib
{
	class CFixedStepScheduler
	{
	public:
		static constexpr int kTicksPerSecond = 60; 			// 초당 고정 업데이트 틱 수
		static constexpr int kMaxUpdatesPerFrame = 4;		// 프레임이 밀렸을 때 최대 몇번까지 실행

		[[nodiscard]] // 이번 실행 FixedUpdate 수와 남은 틱 수를 계산한다.
		int Advance(std::chrono::nanoseconds elapsed) noexcept;

	private:
		// pending된 Update 수를 기록한다.
		std::chrono::nanoseconds::rep m_pendingUpdates = 0;
		// 다음 호출로 넘길 한 틱 미만의 누적값(60배 단위)
		std::chrono::nanoseconds::rep m_remainder = 0;
	};
}