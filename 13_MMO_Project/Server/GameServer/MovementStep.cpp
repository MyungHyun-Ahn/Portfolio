#include "pch.h"
#include "MovementStep.h"
#include <cmath>

namespace GameServer
{
	SMovementStep CalculateMovementStep(SPositionCm current, SPositionCm target, double speedCmPerSecond, double tickSeconds) noexcept
	{
		const double dx = target.m_x - current.m_x;
		const double dy = target.m_y - current.m_y;
		const double distanceCm = std::hypot(dx, dy);
		const double stepCm = speedCmPerSecond * tickSeconds;
		if (distanceCm <= stepCm)
		{
			return { target, true };
		}
		const double ratio = stepCm / distanceCm;
		return { { current.m_x + dx * ratio, current.m_y + dy * ratio }, false };
	}
}
