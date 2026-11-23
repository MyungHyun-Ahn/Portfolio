#pragma once

namespace ClientFrameworkLib::Camera
{
	class CCameraManager
	{
	public:
		// 현재 추적 방식은 대상의 월드 위치에 카메라 중심을 즉시 맞춘다.
		constexpr void FollowTarget(float targetWorldX, float targetWorldY) noexcept
		{
			m_centerWorldX = targetWorldX;
			m_centerWorldY = targetWorldY;
		}

		[[nodiscard]]
		constexpr float GetCenterWorldX() const noexcept { return m_centerWorldX; }
		[[nodiscard]]
		constexpr float GetCenterWorldY() const noexcept { return m_centerWorldY; }

	private:
		float m_centerWorldX = 0.0f;
		float m_centerWorldY = 0.0f;
	};
}