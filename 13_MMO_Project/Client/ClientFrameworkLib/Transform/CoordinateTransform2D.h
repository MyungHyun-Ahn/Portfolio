#pragma once

namespace ClientFrameworkLib::Transform
{
	struct SCoord2D
	{
		float m_x = 0.0f;
		float m_y = 0.0f;
	};

	// 2.1에서는 월드 1단위를 창 내부 좌표 1픽셀로 다룬다.
	[[nodiscard]] constexpr SCoord2D WorldToClient(SCoord2D world, SCoord2D viewCenterWorld, SCoord2D clientCenter) noexcept
	{
		return {world.m_x - viewCenterWorld.m_x + clientCenter.m_x, world.m_y - viewCenterWorld.m_y + clientCenter.m_y};
	}

	[[nodiscard]] constexpr SCoord2D ClientToWorld(SCoord2D client, SCoord2D viewCenterWorld, SCoord2D clientCenter) noexcept
	{
		return {client.m_x - clientCenter.m_x + viewCenterWorld.m_x, client.m_y - clientCenter.m_y + viewCenterWorld.m_y};
	}
}