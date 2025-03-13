#include "pch.h"
#include "BaseEvent.h"
#include "CNetServer.h"
#include "ChatSetting.h"
#include "CPlayer.h"
#include "CSector.h"
#include "CChatServer.h"
#include "SystemEvent.h"

void MonitorTimerEvent::SetEvent() noexcept
{
	execute = std::bind(&MonitorTimerEvent::Execute, this);
	isPQCS = true;
	timeMs = 1000; // 1초
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void MonitorTimerEvent::Execute() noexcept
{
	g_monitor.Update(NET_SERVER::g_NetServer->GetSessionCount(), ((CChatServer *)NET_SERVER::g_NetServer)->GetPlayerCount());
}

void KeyBoardTimerEvent::SetEvent() noexcept
{
	execute = std::bind(&KeyBoardTimerEvent::Execute, this);
	isPQCS = true;
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

void OnAcceptEvent::SetEvent(const UINT64 sessionID) noexcept
{
	execute = std::bind(&OnAcceptEvent::Execute, this, sessionID);
}

void OnAcceptEvent::Execute(const UINT64 sessionID) noexcept
{
	NET_SERVER::g_NetServer->OnAccept(sessionID);
}

void OnClientLeaveEvent::SetEvent(const UINT64 sessionID) noexcept
{
	execute = std::bind(&OnClientLeaveEvent::Execute, this, sessionID);
}

void OnClientLeaveEvent::Execute(const UINT64 sessionID) noexcept
{
	NET_SERVER::g_NetServer->OnClientLeave(sessionID);
}

void SerializableBufferFreeEvent::SetEvent(CDeque<CSerializableBuffer<FALSE> *> *freeQueue) noexcept
{
	execute = std::bind(&SerializableBufferFreeEvent::Execute, this, freeQueue);
}

void SerializableBufferFreeEvent::Execute(CDeque<CSerializableBuffer<FALSE> *> *freeQueue) noexcept
{
	for (auto it = freeQueue->begin(); it != freeQueue->end(); )
	{
		CSerializableBuffer<FALSE>::Free(*it);
		it = freeQueue->erase(it);
	}

	delete freeQueue;
}
