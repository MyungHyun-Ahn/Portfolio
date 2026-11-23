#include <chrono>
#include "CFixedStepScheduler.h"

namespace ClientFrameworkLib
{
	int CFixedStepScheduler::Advance(std::chrono::nanoseconds elapsed) noexcept
	{
		// #1 : 흐른 시간이 0 미만이면 0을 반환한다.
		if (elapsed.count() < 0)
			return 0;

		constexpr auto kNanosecondsPerSecond = std::chrono::nanoseconds(std::chrono::seconds(1)).count();
		const auto elapsedNanoseconds = elapsed.count();

		// #2 : 이번 호출에서 새로 흐른 시간 중 1초 미만 부분을 60배한 값
		const auto scaledElapsed = (elapsedNanoseconds % kNanosecondsPerSecond) * kTicksPerSecond;

		// #3 : 이전 호출에서 남겨둔 m_remainder와 더한 뒤, 합계가 kNanosecondsPerSecond 만큼 채울때마다 1틱으로 침
		// - dueUpdates 	: 1초 미만 부분과 이전 나머지에서 새로 생긴 온전한 틱 수
		// - m_remainder 	: 한 틱 미만의 나머지
		const auto scaledTotal = m_remainder + scaledElapsed;
		const auto dueUpdates = scaledTotal / kNanosecondsPerSecond;
		m_remainder = scaledTotal % kNanosecondsPerSecond;

		// #4 : 온전한 초에서 나온 틱도 더하고, 이번 루프에 넘긴 틱만 대기분에서 뺀다.
		const auto wholeSecondUpdates = (elapsedNanoseconds / kNanosecondsPerSecond) * kTicksPerSecond;
		m_pendingUpdates += wholeSecondUpdates + dueUpdates;
		const int updates = m_pendingUpdates > kMaxUpdatesPerFrame ? kMaxUpdatesPerFrame : static_cast<int>(m_pendingUpdates);
		m_pendingUpdates -= updates;
		return updates;
	}
}