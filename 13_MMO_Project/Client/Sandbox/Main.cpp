#include <Windows.h>
#include <chrono>
#include <memory>
#include <string>
#include "CDirect2DRenderer.h"
#include "CFixedStepScheduler.h"

namespace
{
	constexpr wchar_t kWindowClassName[] = L"PortfolioSandboxWindow";
	constexpr wchar_t kWindowTitle[] = L"Sandbox";

	struct SWindowSizeState
	{
		UINT m_widthPixels = 0;
		UINT m_heightPixels = 0;
		bool m_isMinimized = false;
		bool m_hasPendingResize = false;
	};

	void FixedUpdate(ULONGLONG& updatesInSample) noexcept
	{
		// 예제 콘텐츠는 고정 틱이 실행된 횟수만 센다.
		++updatesInSample;
	}

	// Windows에 등록한 콜백
	LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (message == WM_NCCREATE)
		{
			// main의 창 상태를 HWND에 연결한다. 상태는 창보다 오래 살아 있다.
			const auto* pCreate = reinterpret_cast<const CREATESTRUCTW*>(lParam);
			SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreate->lpCreateParams));
		}

		// 새창 크기와 최소화 상태만 기록한다.
		if (message == WM_SIZE)
		{
			auto* pSize = reinterpret_cast<SWindowSizeState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
			if (pSize != nullptr)
			{
				// 콜백에서는 마지막 상태만 기록하고, 실제 리사이즈는 프레임 사이에 한다.
				pSize->m_isMinimized = wParam == SIZE_MINIMIZED;
				pSize->m_widthPixels = LOWORD(lParam);
				pSize->m_heightPixels = HIWORD(lParam);
				pSize->m_hasPendingResize = true;
				return 0;
			}
		}

		// X 버튼을 누르면 하는 동작을 정의한다.
		if (message == WM_DESTROY)
		{
			// 창이 파괴되면 메시지 루프에도 종료를 알린다.
			PostQuitMessage(0);
			return 0;
		}

		return DefWindowProcW(window, message, wParam, lParam);
	}
}

int main(void)
{
	// 창 만들기 #1 - 현재 실행 중인 exe의 핸들을 가져온다.
	// - 창 클래스를 등록하고 창을 만들 때 넘기는 instance
	const HINSTANCE instance = GetModuleHandleW(nullptr);
	if (instance == nullptr)
		return 1;

	WNDCLASSW windowClass{};
	windowClass.lpfnWndProc = WndProc;
	windowClass.hInstance = instance;

	// 창 만들기 #2 - Windows 기본 화살표 커서를 가져와 이 창 클래스의 기본 커서로 지정함
	windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);

	// 창 만들기 #3 - 창 배경은 Direct2D가 그리므로 클래스 배경 브러시는 지정하지 않는다.
	windowClass.lpszClassName = kWindowClassName;

	// 창 만들기 #4 - WNDCLASSW에 담은 설정을 Windows에 등록한다.
	if (RegisterClassW(&windowClass) == 0)
		return 1;

	SWindowSizeState windowSize;
	const HWND window = CreateWindowExW(0, kWindowClassName, kWindowTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, instance, &windowSize);
	if (window == nullptr)
		return 1;

	// 창 만들기 #5 - Direct2D 랜더러를 초기화한다.
	ClientRenderLib::CDirect2DRenderer renderer;
	if (FAILED(renderer.Initialize(window)))
	{
		DestroyWindow(window);
		return 1;
	}

	// 창 만들기 #6 - 만들어진 창을 화면에 표시한다.
	ShowWindow(window, SW_SHOWDEFAULT);

	MSG message{};
	using Clock = std::chrono::steady_clock;
	ClientFrameworkLib::CFixedStepScheduler fixedStepScheduler;
	auto previousFrameTime = Clock::now();
	auto sampleStartTime = previousFrameTime;
	ULONGLONG framesInSample = 0;
	ULONGLONG updatesInSample = 0;
	while (true)
	{
		// 메시지 처리 #1 - 큐에 있는 메시지를 꺼내고, 비어 있으면 기다리지 않는다.
		while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
		{
			if (message.message == WM_QUIT)
				return static_cast<int>(message.wParam);

			// 메시지 처리 #2 - 키 입력을 바탕으로 필요하면 WM_CHAR 같은 문자 메시지를 큐에 추가한다.
			TranslateMessage(&message);

			// 메시지 처리 #3 - 꺼낸 창 메시지를 대상 창의 콜백으로 전달한다.
			// - 우리가 등록한 WndProc이 호출됨
			DispatchMessageW(&message);
		}

		const auto now = Clock::now();
		const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - previousFrameTime);
		previousFrameTime = now;
		const int updates = fixedStepScheduler.Advance(elapsed);
		for (int tick = 0; tick < updates; ++tick)
			FixedUpdate(updatesInSample);

		// 최소화 중에는 백 버퍼를 건드리지 않고 그리기만 쉰다. 고정 업데이트는 위에서 계속 실행된다.
		if (!windowSize.m_isMinimized && (!windowSize.m_hasPendingResize ||
			(windowSize.m_widthPixels > 0 && windowSize.m_heightPixels > 0)))
		{
			if (windowSize.m_hasPendingResize)
			{
				// 다음 프레임을 그리기 전에 Resize()를 호출한다.
				// - 최소화 중에는 렌더링만 건너뛰고 FixedUpdate는 계속 실행된다.
				windowSize.m_hasPendingResize = false;
				if (FAILED(renderer.Resize(windowSize.m_widthPixels, windowSize.m_heightPixels)))
				{
					DestroyWindow(window);
					return 1;
				}
			}

			renderer.BeginFrame(0.1f, 0.2f, 0.4f);
			renderer.FillRectangle(80.0f, 80.0f, 240.0f, 200.0f);
			if (FAILED(renderer.EndFrame()))
			{
				DestroyWindow(window);
				return 1;
			}
			++framesInSample;
		}

		// 렌더링 프레임과 고정 업데이트의 실행 횟수를 따로 표시한다.
		const auto sampleElapsed = now - sampleStartTime;
		if (sampleElapsed >= std::chrono::seconds(1))
		{
			const double sampleSeconds = std::chrono::duration<double>(sampleElapsed).count();
			const auto fps = static_cast<ULONGLONG>(static_cast<double>(framesInSample) / sampleSeconds);
			const auto ups = static_cast<ULONGLONG>(static_cast<double>(updatesInSample) / sampleSeconds);
			const std::wstring title = L"Sandbox - FPS: " + std::to_wstring(fps) + L" | UPS: " + std::to_wstring(ups);
			SetWindowTextW(window, title.c_str());
			framesInSample = 0;
			updatesInSample = 0;
			sampleStartTime = now;
		}

		// 빈 루프의 바쁜 대기를 줄이기 위한 임시 대기. 고정 틱을 만드는 코드는 아니다.
		Sleep(1);
	}
}