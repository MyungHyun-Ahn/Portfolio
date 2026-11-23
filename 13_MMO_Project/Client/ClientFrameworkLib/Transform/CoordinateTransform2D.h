#pragma once

namespace ClientFrameworkLib::Transform
{
	struct SCoord2D
	{
		float m_x = 0.0f;
		float m_y = 0.0f;
	};

	// 두 변환에 동일한 양수 줌 배율을 사용한다. 1배율에서는 월드 1단위가 1픽셀이다.
	[[nodiscard]]
	constexpr SCoord2D WorldToClient(SCoord2D world, SCoord2D viewCenterWorld, SCoord2D clientCenter, float zoom) noexcept
	{
		return {(world.m_x - viewCenterWorld.m_x) * zoom + clientCenter.m_x,
			(world.m_y - viewCenterWorld.m_y) * zoom + clientCenter.m_y};
	}

	[[nodiscard]]
	constexpr SCoord2D ClientToWorld(SCoord2D client, SCoord2D viewCenterWorld, SCoord2D clientCenter, float zoom) noexcept
	{
		return {(client.m_x - clientCenter.m_x) / zoom + viewCenterWorld.m_x,
			(client.m_y - clientCenter.m_y) / zoom + viewCenterWorld.m_y};
	}
}