#include <chrono>
#include <array>
#include <optional>
#include <unordered_map>
#include "CFixedStepScheduler.h"
#include "Input/CInputManager.h"
#include "Transform/CoordinateTransform2D.h"
#include "Camera/CCameraManager.h"

namespace
{
	using ClientFrameworkLib::Transform::ClientToWorld;
	using ClientFrameworkLib::Transform::WorldToClient;
	using ClientFrameworkLib::Camera::CCameraManager;

	constexpr bool CameraFollowsTarget()
	{
		CCameraManager camera;
		camera.FollowTarget(160.0f, 140.0f);
		if (camera.GetCenterWorldX() != 160.0f || camera.GetCenterWorldY() != 140.0f)
			return false;

		camera.FollowTarget(-25.5f, 200.0f);
		return camera.GetCenterWorldX() == -25.5f && camera.GetCenterWorldY() == 200.0f;
	}

	static_assert(CameraFollowsTarget());

	constexpr auto kClientPoint = WorldToClient({-30.0f, 90.0f}, {100.0f, 40.0f}, {400.0f, 300.0f}, 1.0f);
	static_assert(kClientPoint.m_x == 270.0f && kClientPoint.m_y == 350.0f);
	constexpr auto kWorldPoint = ClientToWorld(kClientPoint, {100.0f, 40.0f}, {400.0f, 300.0f}, 1.0f);
	static_assert(kWorldPoint.m_x == -30.0f && kWorldPoint.m_y == 90.0f);
	constexpr auto kResizedCenter = WorldToClient({100.0f, 40.0f}, {100.0f, 40.0f}, {250.5f, 150.5f}, 2.0f);
	static_assert(kResizedCenter.m_x == 250.5f && kResizedCenter.m_y == 150.5f);

	constexpr bool ZoomPreservesWorldCoordinates()
	{
		CCameraManager camera;
		camera.FollowTarget(100.0f, 40.0f);
		if (camera.GetZoom() != 1.0f)
			return false;

		camera.ZoomIn();
		camera.ZoomIn();
		if (camera.GetZoom() != 2.0f || camera.GetCenterWorldX() != 100.0f || camera.GetCenterWorldY() != 40.0f)
			return false;

		camera.ZoomOut();
		if (camera.GetZoom() != 1.0f)
			return false;
		camera.ZoomOut();
		camera.ZoomOut();
		if (camera.GetZoom() != 0.5f)
			return false;

		for (const float zoom : {0.5f, 1.0f, 2.0f})
		{
			const auto client = WorldToClient({-30.0f, 90.0f}, {100.0f, 40.0f}, {400.0f, 300.0f}, zoom);
			const auto world = ClientToWorld(client, {100.0f, 40.0f}, {400.0f, 300.0f}, zoom);
			if (world.m_x != -30.0f || world.m_y != 90.0f)
				return false;
		}
		return true;
	}

	static_assert(ZoomPreservesWorldCoordinates());

	int TestInputManager()
	{
		using ClientFrameworkLib::Input::CInputManager;
		constexpr unsigned int kMoveTo = 1;
		constexpr unsigned int kMouseLeft = 1;
		constexpr unsigned int kKeyE = 'E';
		CInputManager input;
		const auto unbound = input.GetActionState(kMoveTo);
		if (unbound.m_isDown || unbound.m_wasPressed || unbound.m_wasReleased || unbound.m_pressPosition)
			return 16;

		input.Bind(kMoveTo, kMouseLeft);
		auto state = input.GetActionState(kMoveTo);
		if (state.m_isDown || state.m_wasPressed || state.m_wasReleased)
			return 6;

		input.OnButtonDown(kMouseLeft, CInputManager::SPosition{320, 240});
		input.OnButtonDown(kMouseLeft, CInputManager::SPosition{800, 800});
		input.OnButtonUp(kMouseLeft);
		if (input.GetActionState(kMoveTo).m_wasPressed)
			return 7;
		input.BeginTick();
		const auto tap = input.GetActionState(kMoveTo);
		const auto& pressPosition = tap.m_pressPosition;
		if (!tap.m_wasPressed || !tap.m_wasReleased || tap.m_isDown
			|| !pressPosition || pressPosition->m_xPixels != 320 || pressPosition->m_yPixels != 240)
			return 8;
		input.BeginTick();
		state = input.GetActionState(kMoveTo);
		if (state.m_wasPressed || state.m_wasReleased || state.m_pressPosition)
			return 9;
		if (!tap.m_wasPressed || !tap.m_wasReleased || !tap.m_pressPosition)
			return 17;

		input.Bind(kMoveTo, kKeyE);
		input.OnButtonDown(kMouseLeft);
		input.BeginTick();
		state = input.GetActionState(kMoveTo);
		if (state.m_isDown || state.m_wasPressed)
			return 10;
		input.OnButtonDown(kKeyE);
		input.BeginTick();
		state = input.GetActionState(kMoveTo);
		if (!state.m_isDown || !state.m_wasPressed || state.m_pressPosition)
			return 11;
		input.OnButtonDown(kKeyE);
		input.BeginTick();
		state = input.GetActionState(kMoveTo);
		if (!state.m_isDown || state.m_wasPressed)
			return 12;
		input.OnButtonUp(kKeyE);
		input.BeginTick();
		state = input.GetActionState(kMoveTo);
		if (state.m_isDown || !state.m_wasReleased)
			return 13;
		input.OnButtonDown(kKeyE);
		input.ClearInput();
		input.BeginTick();
		state = input.GetActionState(kMoveTo);
		if (state.m_isDown || state.m_wasPressed)
			return 14;

		input.Bind(kMoveTo, kMouseLeft);
		input.OnButtonDown(kMouseLeft, CInputManager::SPosition{1, 2});
		input.OnButtonUp(kMouseLeft);
		input.OnButtonDown(kMouseLeft, CInputManager::SPosition{3, 4});
		input.BeginTick();
		state = input.GetActionState(kMoveTo);
		const auto& latest = state.m_pressPosition;
		if (!state.m_isDown || !state.m_wasReleased || !latest || latest->m_xPixels != 3 || latest->m_yPixels != 4)
			return 15;
		return 0;
	}
}

static int TestMouseWheel()
{
	ClientFrameworkLib::Input::CInputManager input;
	constexpr int kHalfStep = input.kWheelUnitsPerStep / 2;
	input.OnMouseWheel(kHalfStep);
	input.BeginTick();
	if (input.GetWheelSteps() != 0)
		return 16;
	input.OnMouseWheel(kHalfStep);
	if (input.GetWheelSteps() != 0)
		return 17;
	input.BeginTick();
	if (input.GetWheelSteps() != 1)
		return 18;
	input.BeginTick();
	if (input.GetWheelSteps() != 0)
		return 19;
	input.OnMouseWheel(-5 * kHalfStep);
	input.BeginTick();
	if (input.GetWheelSteps() != -2)
		return 20;
	input.OnMouseWheel(-3 * kHalfStep);
	input.ClearInput();
	if (input.GetWheelSteps() != 0)
		return 21;
	input.OnMouseWheel(-kHalfStep);
	input.BeginTick();
	if (input.GetWheelSteps() != 0)
		return 22;
	return 0;
}

int main(void)
{
	using ClientFrameworkLib::CFixedStepScheduler;
	using namespace std::chrono_literals;

	CFixedStepScheduler accumulated;
	if (accumulated.Advance(8ms) != 0 || accumulated.Advance(9ms) != 1)
		return 1;

	CFixedStepScheduler capped;
	if (capped.Advance(210ms) != 4 || capped.Advance(0ns) != 4 || capped.Advance(0ns) != 4
		|| capped.Advance(0ns) != 0 || capped.Advance(7ms) != 1)
		return 2;

	CFixedStepScheduler exact;
	int updates = 0;
	for (int frame = 0; frame < 100; ++frame)
		updates += exact.Advance(10ms);
	if (updates != 60)
		return 3;

	CFixedStepScheduler paused;
	if (paused.Advance(1010ms) != 4)
		return 4;
	for (int batch = 1; batch < 15; ++batch)
	{
		if (paused.Advance(0ns) != 4)
			return 4;
	}
	if (paused.Advance(0ns) != 0 || paused.Advance(7ms) != 1)
		return 4;

	CFixedStepScheduler invalid;
	if (invalid.Advance(10ms) != 0 || invalid.Advance(-10ms) != 0 || invalid.Advance(7ms) != 1)
		return 5;
	if (const int inputResult = TestInputManager(); inputResult != 0)
		return inputResult;
	return TestMouseWheel();
}