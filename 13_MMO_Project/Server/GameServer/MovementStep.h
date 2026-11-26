#pragma once

namespace GameServer
{
	struct SPositionCm
	{
		double m_x{};
		double m_y{};
	};

	struct SMovementStep
	{
		SPositionCm m_position;
		bool m_isArrived{};
	};

	// 유한한 맵 좌표와 음이 아닌 속도·시간으로 목적지 방향의 한 스텝을 계산한다.
	[[nodiscard]]
	SMovementStep CalculateMovementStep(SPositionCm current, SPositionCm target, double speedCmPerSecond, double tickSeconds) noexcept;
}
