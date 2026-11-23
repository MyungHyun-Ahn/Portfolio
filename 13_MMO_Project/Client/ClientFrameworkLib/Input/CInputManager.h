#pragma once

namespace ClientFrameworkLib::Input
{
	// 메시지 수집과 고정 틱이 같은 스레드에서 실행되는 것을 전제로 한다.
	class CInputManager
	{
	public:
		using ActionId = unsigned int;
		using ButtonId = unsigned int;

		struct SPosition
		{
			int m_xPixels;
			int m_yPixels;
		};

		struct SActionState
		{
			bool m_isDown = false;
			bool m_wasPressed = false;
			bool m_wasReleased = false;
			std::optional<SPosition> m_pressPosition;
		};

		// 한 액션에 버튼 하나를 연결한다. 다시 호출하면 해당 액션의 매핑을 교체한다.
		void Bind(ActionId action, ButtonId button);
		void OnButtonDown(ButtonId button, std::optional<SPosition> position = std::nullopt) noexcept;
		void OnButtonUp(ButtonId button) noexcept;
		void ClearButtons() noexcept;
		// 고정 틱마다 한 번 호출해 대기 중인 눌림·떼어짐을 해당 틱에만 노출한다.
		void BeginTick() noexcept;

		// 현재 눌림 상태와 이번 틱의 변화를 값으로 반환한다. 미매핑 액션은 비활성 상태다.
		[[nodiscard]]
		SActionState GetActionState(ActionId action) const noexcept;

	private:
		struct STransitions
		{
			bool m_wasPressed = false;
			bool m_wasReleased = false;
			std::optional<SPosition> m_pressPosition;
		};

		struct SButtonState
		{
			bool m_isDown = false;
			STransitions m_pending;
			STransitions m_thisTick;
		};

	private:
		// Sandbox는 Win32 가상 키 값을 키보드와 마우스 버튼의 공통 ID로 사용한다.
		static constexpr unsigned int kButtonCount = 256;
		std::array<SButtonState, kButtonCount> m_buttons{};
		std::unordered_map<ActionId, ButtonId> m_bindings;
	};
}