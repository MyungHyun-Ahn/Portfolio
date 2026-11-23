#include <array>
#include <chrono>
#include <cmath>
#include <optional>
#include "CFixedStepScheduler.h"
#include "Animation/CSpriteAnimator.h"
#include "Transform/CoordinateTransform2D.h"
#include "CSandboxPlayer.h"

namespace Sandbox
{
	using ClientFrameworkLib::Transform::SCoord2D;

	namespace
	{
		constexpr float kMoveWorldUnitsPerTick = 300.0f / ClientFrameworkLib::CFixedStepScheduler::kTicksPerSecond;
		constexpr unsigned int kWalkFrameCount = 4;
		constexpr unsigned int kSpriteTicksPerFrame = 6;
		// Walk 시트의 열은 상·하·좌·우 순서다.
		constexpr std::array<unsigned int, 4> kWalkColumnsByFacing{1, 0, 2, 3};
	}

	void CSandboxPlayer::MoveTo(SCoord2D destinationWorld) noexcept
	{
		m_destinationWorld = destinationWorld;
		const float deltaX = destinationWorld.m_x - m_centerWorld.m_x;
		const float deltaY = destinationWorld.m_y - m_centerWorld.m_y;
		if (deltaX != 0.0f || deltaY != 0.0f)
		{
			// 직선 이동을 시작할 때 방향을 정한다. 대각선이 정확히 같으면 세로 방향을 우선한다.
			m_facing = std::abs(deltaX) > std::abs(deltaY)
				? (deltaX < 0.0f ? EFacing::Left : EFacing::Right)
				: (deltaY < 0.0f ? EFacing::Up : EFacing::Down);
		}
	}

	void CSandboxPlayer::FixedUpdate() noexcept
	{
		advanceMovement();
		const auto nextClip = m_destinationWorld ? ESpriteClip::Walk : ESpriteClip::Idle;
		if (nextClip != m_clip)
		{
			m_clip = nextClip;
			if (m_clip == ESpriteClip::Walk)
				m_animator.Restart(kWalkFrameCount, kSpriteTicksPerFrame);
		}
		else if (m_clip == ESpriteClip::Walk)
		{
			m_animator.AdvanceTick();
		}
	}

	SCoord2D CSandboxPlayer::GetCenterWorld() const noexcept
	{
		return m_centerWorld;
	}

	std::optional<SCoord2D> CSandboxPlayer::GetDestinationWorld() const noexcept
	{
		return m_destinationWorld;
	}

	SPlayerSpriteFrame CSandboxPlayer::GetSpriteFrame() const noexcept
	{
		const auto facingIndex = static_cast<unsigned int>(m_facing);
		if (m_clip == ESpriteClip::Walk)
			return {m_clip, kWalkColumnsByFacing[facingIndex], m_animator.GetFrameIndex()};

		return {m_clip, facingIndex, 0u};
	}

	void CSandboxPlayer::advanceMovement() noexcept
	{
		if (!m_destinationWorld)
			return;

		const SCoord2D target = *m_destinationWorld;
		const float deltaX = target.m_x - m_centerWorld.m_x;
		const float deltaY = target.m_y - m_centerWorld.m_y;
		const float distanceWorld = std::hypot(deltaX, deltaY);

		// 한 틱 이동 거리 안에 목적지가 있으면 지나치지 말고 그 자리에서 멈춘다.
		if (distanceWorld <= kMoveWorldUnitsPerTick)
		{
			m_centerWorld = target;
			m_destinationWorld.reset();
			return;
		}

		const float stepRatio = kMoveWorldUnitsPerTick / distanceWorld;
		m_centerWorld.m_x += deltaX * stepRatio;
		m_centerWorld.m_y += deltaY * stepRatio;
	}
}