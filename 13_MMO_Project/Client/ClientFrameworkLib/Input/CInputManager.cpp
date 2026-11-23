#include <array>
#include <optional>
#include <unordered_map>
#include "CInputManager.h"

namespace ClientFrameworkLib::Input
{
	void CInputManager::Bind(ActionId action, ButtonId button)
	{
		if (button < m_buttons.size())
			m_bindings.insert_or_assign(action, button);
	}

	void CInputManager::OnButtonDown(ButtonId button, std::optional<SPosition> position) noexcept
	{
		if (button >= m_buttons.size())
			return;

		auto& state = m_buttons[button];
		if (state.m_isDown)
			return;

		state.m_isDown = true;
		state.m_pending.m_wasPressed = true;
		state.m_pending.m_pressPosition = position;
	}

	void CInputManager::OnButtonUp(ButtonId button) noexcept
	{
		if (button >= m_buttons.size())
			return;

		auto& state = m_buttons[button];
		if (!state.m_isDown)
			return;

		state.m_isDown = false;
		state.m_pending.m_wasReleased = true;
	}

	void CInputManager::OnMouseWheel(int delta) noexcept
	{
		m_pendingWheelDelta += delta;
	}

	void CInputManager::ClearInput() noexcept
	{
		m_buttons.fill(SButtonState{});
		m_pendingWheelDelta = 0;
		m_wheelStepsThisTick = 0;
	}

	void CInputManager::BeginTick() noexcept
	{
		for (auto& state : m_buttons)
		{
			state.m_thisTick = state.m_pending;
			state.m_pending = {};
		}
		m_wheelStepsThisTick = m_pendingWheelDelta / kWheelUnitsPerStep;
		// 한 스텝 미만의 델타는 다음 휠 입력까지 보존한다.
		m_pendingWheelDelta %= kWheelUnitsPerStep;
	}

	CInputManager::SActionState CInputManager::GetActionState(ActionId action) const noexcept
	{
		const auto binding = m_bindings.find(action);
		if (binding == m_bindings.end())
			return {};

		const auto& button = m_buttons[binding->second];
		const auto& transitions = button.m_thisTick;
		return {button.m_isDown, transitions.m_wasPressed, transitions.m_wasReleased, transitions.m_pressPosition};
	}
}