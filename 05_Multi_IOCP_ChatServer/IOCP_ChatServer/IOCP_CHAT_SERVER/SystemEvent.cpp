#include "pch.h"
#include "BaseEvent.h"
#include "CNetServer.h"
#include "CNetSession.h"
#include "ChatSetting.h"
#include "CPlayer.h"
#include "CSector.h"
#include "CChatServer.h"
#include "SystemEvent.h"

void MonitorEvent::SetEvent() noexcept
{
	execute = std::bind(&MonitorEvent::Execute, this);
	timeMs = 1000; // 1초
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void MonitorEvent::Execute() noexcept
{
	g_monitor.Update(g_NetServer->GetSessionCount(), ((CChatServer *)g_NetServer)->GetPlayerCount());
}

void KeyBoardEvent::SetEvent() noexcept
{
	execute = std::bind(&KeyBoardEvent::Execute, this);
	timeMs = 1000; // 1초
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void KeyBoardEvent::Execute() noexcept
{
	// 서버 종료
	if (GetAsyncKeyState(VK_F1))
		g_NetServer->Stop();

	// 프로파일러 저장
	if (GetAsyncKeyState(VK_F2))
		g_ProfileMgr->DataOutToFile();
}
