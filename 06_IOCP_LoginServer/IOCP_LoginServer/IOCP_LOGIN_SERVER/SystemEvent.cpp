#include "pch.h"
#include "BaseEvent.h"
#include "CNetServer.h"
#include "CNetSession.h"
#include "SystemEvent.h"

void MonitorTimerEvent::SetEvent() noexcept
{
	execute = std::bind(&MonitorTimerEvent::Execute, this);
	timeMs = 1000; // 1��
	nextExecuteTime = timeGetTime(); // ���� �ð�
}

void MonitorTimerEvent::Execute() noexcept
{
	g_monitor.Update(g_NetServer->GetSessionCount());
}

void KeyBoardTimerEvent::SetEvent() noexcept
{
	execute = std::bind(&KeyBoardTimerEvent::Execute, this);
	timeMs = 1000; // 1��
	nextExecuteTime = timeGetTime(); // ���� �ð�
}

void KeyBoardTimerEvent::Execute() noexcept
{
	// ���� ����
	if (GetAsyncKeyState(VK_F1))
		g_NetServer->Stop();

	// �������Ϸ� ����
	if (GetAsyncKeyState(VK_F2))
		g_ProfileMgr->DataOutToFile();
}
