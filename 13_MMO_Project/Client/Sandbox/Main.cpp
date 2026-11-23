#include <Windows.h>
#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include "CDirect2DRenderer.h"
#include "CFixedStepScheduler.h"
#include "Input/CInputManager.h"
#include "Input/Win32InputAdapter.h"
#include "Camera/CCameraManager.h"
#include "Animation/CSpriteAnimator.h"
#include "Transform/CoordinateTransform2D.h"
#include "CSandboxPlayer.h"

namespace
{
	constexpr wchar_t kWindowClassName[] = L"PortfolioSandboxWindow";
	constexpr wchar_t kWindowTitle[] = L"Sandbox";
	using ClientFrameworkLib::Input::CInputManager;
	using ClientFrameworkLib::CFixedStepScheduler;
	using ClientFrameworkLib::Camera::CCameraManager;
	using ClientFrameworkLib::Transform::SCoord2D;
	using ClientFrameworkLib::Transform::WorldToClient;
	using ClientFrameworkLib::Transform::ClientToWorld;
	using Sandbox::CSandboxPlayer;
	using Sandbox::ESpriteClip;

	enum class EGameAction : CInputManager::ActionId
	{
		MoveTo = 1,
	};

	constexpr SCoord2D kWorldLandmark{460.0f, 240.0f};
	constexpr float kSpriteFramePixels = 16.0f;
	constexpr float kSpriteHalfSizeWorld = 16.0f;
	constexpr float kSpriteOffsetWorldY = -20.0f;

	struct SWindowSizeState
	{
		UINT m_widthPixels = 0;
		UINT m_heightPixels = 0;
		bool m_isMinimized = false;
		bool m_hasPendingResize = false;
	};

	struct SWindowState
	{
		SWindowSizeState m_size;
		CInputManager m_input;
	};

	void ApplyPlayerInput(const CInputManager& input, CSandboxPlayer& player, SCoord2D viewCenterWorld,
		SCoord2D clientCenter, float zoom) noexcept
	{
		const auto moveTo = input.GetActionState(static_cast<CInputManager::ActionId>(EGameAction::MoveTo));
		if (moveTo.m_wasPressed && moveTo.m_pressPosition)
		{
			const auto& press = *moveTo.m_pressPosition;
			const SCoord2D click{static_cast<float>(press.m_xPixels), static_cast<float>(press.m_yPixels)};
			player.MoveTo(ClientToWorld(click, viewCenterWorld, clientCenter, zoom));
		}
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

		auto* pState = reinterpret_cast<SWindowState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
		if (pState != nullptr)
		{
			if (ClientFrameworkLib::Input::HandleInputMessage(pState->m_input, window, message, wParam, lParam))
				return 0;

			if (message == WM_SIZE)
			{
				// 콜백에서는 마지막 상태만 기록하고, 실제 리사이즈는 프레임 사이에 한다.
				pState->m_size.m_isMinimized = wParam == SIZE_MINIMIZED;
				pState->m_size.m_widthPixels = LOWORD(lParam);
				pState->m_size.m_heightPixels = HIWORD(lParam);
				pState->m_size.m_hasPendingResize = true;
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

	SWindowState windowState;
	windowState.m_input.Bind(static_cast<CInputManager::ActionId>(EGameAction::MoveTo), VK_LBUTTON);

	const HWND window = CreateWindowExW(0, kWindowClassName, kWindowTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, instance, &windowState);
	if (window == nullptr)
		return 1;

	// 창 만들기 #5 - Direct2D 랜더러를 초기화한다.
	ClientRenderLib::CDirect2DRenderer renderer;
	if (FAILED(renderer.Initialize(window)))
	{
		DestroyWindow(window);
		return 1;
	}

	// 작업 디렉터리와 관계없이 실행 파일 옆에 배치한 PNG를 한 번만 읽는다.
	std::array<wchar_t, 32768> modulePath{};
	const DWORD modulePathLength = GetModuleFileNameW(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
	if (modulePathLength == 0 || modulePathLength >= modulePath.size())
	{
		DestroyWindow(window);
		return 2;
	}
	const auto spriteDirectory = std::filesystem::path(modulePath.data()).parent_path();
	if (FAILED(renderer.LoadSpritePng(static_cast<unsigned int>(ESpriteClip::Idle), (spriteDirectory / L"BoyIdle.png").c_str())) ||
		FAILED(renderer.LoadSpritePng(static_cast<unsigned int>(ESpriteClip::Walk), (spriteDirectory / L"BoyWalk.png").c_str())))
	{
		DestroyWindow(window);
		return 2;
	}

	// 창 만들기 #6 - 만들어진 창을 화면에 표시한다.
	ShowWindow(window, SW_SHOWDEFAULT);

	MSG message{};
	using Clock = std::chrono::steady_clock;
	CFixedStepScheduler fixedStepScheduler;
	CSandboxPlayer player;
	CCameraManager cameraManager;
	const auto initialCenterWorld = player.GetCenterWorld();
	cameraManager.FollowTarget(initialCenterWorld.m_x, initialCenterWorld.m_y);
	auto& windowSize = windowState.m_size;
	RECT initialClient{};
	if (!GetClientRect(window, &initialClient))
	{
		DestroyWindow(window);
		return 1;
	}
	SCoord2D clientCenter{static_cast<float>(initialClient.right) * 0.5f, static_cast<float>(initialClient.bottom) * 0.5f};
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
		if (windowSize.m_hasPendingResize && !windowSize.m_isMinimized && windowSize.m_widthPixels > 0 && windowSize.m_heightPixels > 0)
			clientCenter = {static_cast<float>(windowSize.m_widthPixels) * 0.5f, static_cast<float>(windowSize.m_heightPixels) * 0.5f};

		const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - previousFrameTime);
		previousFrameTime = now;
		const int updates = fixedStepScheduler.Advance(elapsed);
		for (int tick = 0; tick < updates; ++tick)
		{
			windowState.m_input.BeginTick();
			const SCoord2D viewCenterWorld{cameraManager.GetCenterWorldX(), cameraManager.GetCenterWorldY()};
			// 같은 틱의 클릭은 줌 변경 전 배율로 해석한다.
			ApplyPlayerInput(windowState.m_input, player, viewCenterWorld, clientCenter, cameraManager.GetZoom());
			player.FixedUpdate();
			++updatesInSample;
			const auto playerCenterWorld = player.GetCenterWorld();
			cameraManager.FollowTarget(playerCenterWorld.m_x, playerCenterWorld.m_y);
			const auto wheelSteps = windowState.m_input.GetWheelSteps();
			for (auto steps = wheelSteps; steps > 0; --steps)
				cameraManager.ZoomIn();
			for (auto steps = wheelSteps; steps < 0; ++steps)
				cameraManager.ZoomOut();
		}

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
			const SCoord2D viewCenterWorld{cameraManager.GetCenterWorldX(), cameraManager.GetCenterWorldY()};
			const float zoom = cameraManager.GetZoom();
			const auto landmark = WorldToClient(kWorldLandmark, viewCenterWorld, clientCenter, zoom);
			const float landmarkHalfSize = 5.0f * zoom;
			renderer.FillRectangle(landmark.m_x - landmarkHalfSize, landmark.m_y - landmarkHalfSize,
				landmark.m_x + landmarkHalfSize, landmark.m_y + landmarkHalfSize);
			if (const auto destinationWorld = player.GetDestinationWorld())
			{
				const auto target = WorldToClient(*destinationWorld, viewCenterWorld, clientCenter, zoom);
				renderer.FillRectangle(target.m_x - 4.0f, target.m_y - 4.0f, target.m_x + 4.0f, target.m_y + 4.0f);
			}
			const auto playerCenterWorld = player.GetCenterWorld();
			const auto center = WorldToClient(playerCenterWorld, viewCenterWorld, clientCenter, zoom);
			const float halfWidth = 80.0f * zoom;
			const float halfHeight = 60.0f * zoom;
			renderer.FillRectangle(center.m_x - halfWidth, center.m_y - halfHeight, center.m_x + halfWidth, center.m_y + halfHeight);
			const SCoord2D spriteWorld{playerCenterWorld.m_x, playerCenterWorld.m_y + kSpriteOffsetWorldY};
			const auto spriteCenter = WorldToClient(spriteWorld, viewCenterWorld, clientCenter, zoom);
			const float spriteHalfSize = kSpriteHalfSizeWorld * zoom;
			const auto spriteFrame = player.GetSpriteFrame();
			const float sourceLeftPixels = static_cast<float>(spriteFrame.m_column) * kSpriteFramePixels;
			const float sourceTopPixels = static_cast<float>(spriteFrame.m_row) * kSpriteFramePixels;
			renderer.DrawSprite(static_cast<unsigned int>(spriteFrame.m_clip),
				spriteCenter.m_x - spriteHalfSize, spriteCenter.m_y - spriteHalfSize,
				spriteCenter.m_x + spriteHalfSize, spriteCenter.m_y + spriteHalfSize,
				{sourceLeftPixels, sourceTopPixels, sourceLeftPixels + kSpriteFramePixels, sourceTopPixels + kSpriteFramePixels});
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