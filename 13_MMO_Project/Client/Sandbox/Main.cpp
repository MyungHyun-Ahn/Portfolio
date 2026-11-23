#include <Windows.h>
#include <string>

namespace
{
	constexpr wchar_t kWindowClassName[] = L"PortfolioSandboxWindow";
	constexpr wchar_t kWindowTitle[] = L"Sandbox";

	// Windows에 등록한 콜백
	LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
	{
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

	// 창 만들기 #3 - 시스템의 기본 창 배경색으로 칠하는 브러시를 가져온다.
	windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	windowClass.lpszClassName = kWindowClassName;

	// 창 만들기 #4 - WNDCLASSW에 담은 설정을 Windows에 등록한다.
	if (RegisterClassW(&windowClass) == 0)
		return 1;

	const HWND window = CreateWindowExW(0, kWindowClassName, kWindowTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, instance, nullptr);
	if (window == nullptr)
		return 1;

	// 창 만들기 #5 - 만들어진 창을 화면에 표시한다.
	ShowWindow(window, SW_SHOWDEFAULT);

	MSG message{};
	ULONGLONG framesInSample = 0;
	ULONGLONG sampleStartMilliseconds = GetTickCount64();
	constexpr ULONGLONG kSampleIntervalMilliseconds = 1000;
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

		// 이번 측정 구간의 초당 루프 횟수를 표시한다.
		++framesInSample;
		const ULONGLONG elapsedMilliseconds = GetTickCount64() - sampleStartMilliseconds;
		if (elapsedMilliseconds >= kSampleIntervalMilliseconds)
		{
			const ULONGLONG fps = framesInSample * 1000 / elapsedMilliseconds;
			const std::wstring title = L"Sandbox - FPS: " + std::to_wstring(fps);
			SetWindowTextW(window, title.c_str());
			framesInSample = 0;
			sampleStartMilliseconds += elapsedMilliseconds;
		}

		// 빈 루프의 바쁜 대기를 줄이기 위한 임시 대기. 고정 틱을 만드는 코드는 아니다.
		Sleep(1);
	}
}