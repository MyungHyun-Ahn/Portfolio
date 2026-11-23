#pragma once

namespace Sandbox
{
	enum class ESpriteClip : unsigned int
	{
		Idle,
		Walk,
	};

	struct SPlayerSpriteFrame
	{
		ESpriteClip m_clip;
		unsigned int m_column;
		unsigned int m_row;
	};

	class CSandboxPlayer
	{
	public:
		void MoveTo(ClientFrameworkLib::Transform::SCoord2D destinationWorld) noexcept;
		void FixedUpdate() noexcept;

		[[nodiscard]]
		ClientFrameworkLib::Transform::SCoord2D GetCenterWorld() const noexcept;
		[[nodiscard]]
		std::optional<ClientFrameworkLib::Transform::SCoord2D> GetDestinationWorld() const noexcept;
		[[nodiscard]]
		SPlayerSpriteFrame GetSpriteFrame() const noexcept;

	private:
		void advanceMovement() noexcept;

	private:
		// Idle 시트의 열 순서다.
		enum class EFacing : unsigned int
		{
			Down,
			Up,
			Left,
			Right,
		};

		ClientFrameworkLib::Transform::SCoord2D m_centerWorld{160.0f, 140.0f};
		std::optional<ClientFrameworkLib::Transform::SCoord2D> m_destinationWorld;
		EFacing m_facing = EFacing::Down;
		ESpriteClip m_clip = ESpriteClip::Idle;
		ClientFrameworkLib::Animation::CSpriteAnimator m_animator;
	};
}