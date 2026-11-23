#include <Windows.h>
#include <Windowsx.h>
#include <array>
#include <optional>
#include <unordered_map>
#include "CInputManager.h"
#include "Win32InputAdapter.h"

namespace ClientFrameworkLib::Input
{
	bool HandleInputMessage(CInputManager& input, HWND window, UINT message, WPARAM wParam, LPARAM lParam) noexcept
	{
		switch (message)
		{
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			// 포커스가 돌아온 뒤 도착한 자동 반복도 새 입력으로 등록하지 않는다.
			if ((lParam & (1L << 30)) == 0)
				input.OnButtonDown(static_cast<CInputManager::ButtonId>(wParam));
			return message == WM_KEYDOWN;
		case WM_KEYUP:
		case WM_SYSKEYUP:
			input.OnButtonUp(static_cast<CInputManager::ButtonId>(wParam));
			return message == WM_KEYUP;
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		{
			const auto button = message == WM_LBUTTONDOWN ? VK_LBUTTON : message == WM_RBUTTONDOWN ? VK_RBUTTON : VK_MBUTTON;
			input.OnButtonDown(button, CInputManager::SPosition{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
			SetCapture(window);
			return true;
		}
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		{
			const auto button = message == WM_LBUTTONUP ? VK_LBUTTON : message == WM_RBUTTONUP ? VK_RBUTTON : VK_MBUTTON;
			input.OnButtonUp(button);
			if ((wParam & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)) == 0 && GetCapture() == window)
				ReleaseCapture();
			return true;
		}
		case WM_CAPTURECHANGED:
			input.OnButtonUp(VK_LBUTTON);
			input.OnButtonUp(VK_RBUTTON);
			input.OnButtonUp(VK_MBUTTON);
			return true;
		case WM_KILLFOCUS:
			input.ClearButtons();
			if (GetCapture() == window)
				ReleaseCapture();
			break;
		}
		return false;
	}
}
