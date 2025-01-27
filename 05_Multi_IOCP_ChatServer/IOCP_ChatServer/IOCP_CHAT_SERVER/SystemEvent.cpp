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
	timeMs = 1000; // 1��
	nextExecuteTime = timeGetTime(); // ���� �ð�
}

void MonitorEvent::Execute() noexcept
{
	g_monitor.Update(g_NetServer->GetSessionCount(), ((CChatServer *)g_NetServer)->GetPlayerCount());
}

void KeyBoardEvent::SetEvent() noexcept
{
	execute = std::bind(&KeyBoardEvent::Execute, this);
	timeMs = 1000; // 1��
	nextExecuteTime = timeGetTime(); // ���� �ð�
}

void KeyBoardEvent::Execute() noexcept
{
	// ���� ����
	if (GetAsyncKeyState(VK_F1))
		g_NetServer->Stop();

	// �������Ϸ� ����
	if (GetAsyncKeyState(VK_F2))
		g_ProfileMgr->DataOutToFile();
}
