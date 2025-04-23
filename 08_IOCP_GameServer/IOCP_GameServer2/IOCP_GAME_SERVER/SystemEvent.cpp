#include "pch.h"
#include "ServerSetting.h"
#include "BaseEvent.h"
#include "CNetServer.h"
#include "CGameServer.h"
#include "SystemEvent.h"
#include "CBaseContents.h"

void MonitorTimerEvent::SetEvent() noexcept
{
	// execute = std::bind(&MonitorTimerEvent::Execute, this);
	isRunning = true;
	isTimerEvent = true;
	timeMs = 1000; // 1초
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void MonitorTimerEvent::execute(int delayFrame) noexcept
{
	g_monitor.Update();
}

void KeyBoardTimerEvent::SetEvent() noexcept
{
	// execute = std::bind(&KeyBoardTimerEvent::Execute, this);
	isRunning = true;
	isTimerEvent = true;
	timeMs = 1000; // 1초
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void KeyBoardTimerEvent::execute(int delayFrame) noexcept
{
	// 서버 종료
	if (GetAsyncKeyState(VK_F1))
		NET_SERVER::g_NetServer->Stop();

	// 프로파일러 저장
	if (GetAsyncKeyState(VK_F2))
		g_ProfileMgr->DataOutToFile();
}

ContentsFrameEvent::~ContentsFrameEvent()
{
	delete m_pBaseContent;
}

void ContentsFrameEvent::SetEvent(CBaseContents *pBaseContent, int frame) noexcept
{
	m_pBaseContent = pBaseContent;
	// TimerEvent의 종료를 제어하기 위함
	m_pBaseContent->m_pTimerEvent = this;
	if (frame == 0)
		timeMs = 0;
	else
		timeMs = 1000 / frame;
	nextExecuteTime = timeGetTime();
}

// delayFrame 안씀

void ContentsFrameEvent::execute(int delayFrame) noexcept
{
	InterlockedIncrement(&m_pBaseContent->m_FPS);

	m_pBaseContent->ProcessMoveJob();
	m_pBaseContent->ProcessLeaveJob();
	m_pBaseContent->ProcessRecvMsg(delayFrame);
}

void SendAllTimerEvent::SetEvent() noexcept
{
	timeMs = 50; // 1초에 20번
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void SendAllTimerEvent::execute(int delayFrame) noexcept
{
	NET_SERVER::g_NetServer->SendAll();
}

void ExampleTimerEvent::SetEvent(int param1, double param2, int frame) noexcept
{
	m_iParam1 = param1;
	m_dParam2 = param2;
	if (frame == 0)
		timeMs = 0;
	else
		timeMs = 1000 / frame;
	nextExecuteTime = timeGetTime();
}

void ExampleTimerEvent::execute(int delayFrame) noexcept
{
	m_iParam1 += (1 + (delayFrame * 1));
	// ...
}
