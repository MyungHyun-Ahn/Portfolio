#pragma once

namespace ClientFrameworkLib::Input
{
	class CInputManager;

	[[nodiscard]] // true를 반환한 메시지만 WndProc에서 기본 처리를 생략한다.
	bool HandleInputMessage(CInputManager& input, HWND window, UINT message, WPARAM wParam, LPARAM lParam) noexcept;
}