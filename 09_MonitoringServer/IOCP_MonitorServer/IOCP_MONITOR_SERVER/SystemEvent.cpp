#include "pch.h"
#include "BaseEvent.h"
#include "CNetServer.h"
#include "SystemEvent.h"

void MonitorTimerEvent::SetEvent() noexcept
{
	execute = std::bind(&MonitorTimerEvent::Execute, this);
	timeMs = 1000; // 1초
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void MonitorTimerEvent::Execute() noexcept
{
	g_monitor.Update(NET_SERVER::g_NetServer->GetSessionCount());
}

void KeyBoardTimerEvent::SetEvent() noexcept
{
	execute = std::bind(&KeyBoardTimerEvent::Execute, this);
	timeMs = 1000; // 1초
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void KeyBoardTimerEvent::Execute() noexcept
{
	// 서버 종료
	if (GetAsyncKeyState(VK_F1))
		NET_SERVER::g_NetServer->Stop();

	// 프로파일러 저장
	if (GetAsyncKeyState(VK_F2))
		g_ProfileMgr->DataOutToFile();
}
