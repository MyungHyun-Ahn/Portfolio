#pragma once

namespace ClientFrameworkLib::Animation
{
	class CSpriteAnimator
	{
	public:
		// 클립 전환 시 호출한다. 프레임 수와 프레임당 틱 수는 최소 1로 처리한다.
		constexpr void Restart(unsigned int frameCount, unsigned int ticksPerFrame) noexcept
		{
			m_frameCount = frameCount == 0 ? 1 : frameCount;
			m_ticksPerFrame = ticksPerFrame == 0 ? 1 : ticksPerFrame;
			m_frameIndex = 0;
			m_elapsedTicks = 0;
		}

		constexpr void AdvanceTick() noexcept
		{
			if (++m_elapsedTicks == m_ticksPerFrame)
			{
				m_elapsedTicks = 0;
				m_frameIndex = (m_frameIndex + 1) % m_frameCount;
			}
		}

		[[nodiscard]]
		constexpr unsigned int GetFrameIndex() const noexcept { return m_frameIndex; }

	private:
		unsigned int m_frameCount = 1;
		unsigned int m_ticksPerFrame = 1;
		unsigned int m_frameIndex = 0;
		unsigned int m_elapsedTicks = 0;
	};
}