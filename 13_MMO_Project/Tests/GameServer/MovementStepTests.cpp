#include <cmath>
#include <cstdio>
#include "../../Server/GameServer/MovementStep.h"

namespace
{
	struct SCase
	{
		const char *pName;
		GameServer::SPositionCm start, target;
		double speed, seconds;
		GameServer::SPositionCm expected;
		bool arrived;
	};
}

// 각 입력의 한 틱 뒤 좌표와 도착 여부를 실제 서버 이동 함수와 대조한다.
int main(void)
{
	const SCase cases[] = {
		{ "same", { 4, -2 }, { 4, -2 }, 20, 0.5, { 4, -2 }, true },
		{ "axis", { 0, 0 }, { 50, 0 }, 20, 0.5, { 10, 0 }, false },
		{ "diagonal", { 0, 0 }, { 30, 40 }, 20, 0.5, { 6, 8 }, false },
		{ "negative", { 8, 10 }, { -22, -30 }, 20, 0.25, { 5, 6 }, false },
		{ "exact", { 1, 2 }, { 7, 10 }, 20, 0.5, { 7, 10 }, true },
		{ "near", { 0, 0 }, { 4, 0 }, 20, 0.5, { 4, 0 }, true },
		{ "zero_speed", { 1, 2 }, { 7, 10 }, 0, 0.5, { 1, 2 }, false },
		{ "zero_time", { 1, 2 }, { 7, 10 }, 20, 0, { 1, 2 }, false }
	};
	int failures = 0;
	for (const SCase &item : cases)
	{
		const auto step = GameServer::CalculateMovementStep(item.start, item.target, item.speed, item.seconds);
		const double error = std::hypot(step.m_position.m_x - item.expected.m_x, step.m_position.m_y - item.expected.m_y);
		if (!(error <= (item.arrived ? 0.0 : 1e-12)) || step.m_isArrived != item.arrived)
		{
			std::fprintf(stderr, "FAIL %s\n", item.pName);
			++failures;
		}
	}
	return failures == 0 ? 0 : 1;
}
