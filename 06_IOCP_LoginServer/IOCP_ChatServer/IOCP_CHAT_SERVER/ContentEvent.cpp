#include "pch.h"
#include "BaseEvent.h"
#include "CNetServer.h"
#include "CNetSession.h"
#include "ChatSetting.h"
#include "CPlayer.h"
#include "CSector.h"
#include "CChatServer.h"
#include "ContentEvent.h"

void SectorBroadcastTimerEvent::SetEvent() noexcept
{
	execute = std::bind(&SectorBroadcastTimerEvent::Execute, this);
	isPQCS = false;
	timeMs = SECTOR_BROATCAST_TIMER_TICK;
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void SectorBroadcastTimerEvent::Execute() noexcept
{
	CChatServer *chatServer = (CChatServer *)g_NetServer;
	chatServer->SectorBroadcast();
}

void NonLoginHeartBeatTimerEvent::SetEvent() noexcept
{
	execute = std::bind(&NonLoginHeartBeatTimerEvent::Execute, this);
	isPQCS = false;
	timeMs = NON_LOGIN_TIME_OUT_CHECK;
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void NonLoginHeartBeatTimerEvent::Execute() noexcept
{
	CChatServer *chatServer = (CChatServer *)g_NetServer;
	chatServer->NonLoginHeartBeat();
}

void LoginHeartBeatTimerEvent::SetEvent() noexcept
{
	execute = std::bind(&LoginHeartBeatTimerEvent::Execute, this);
	isPQCS = false;
	timeMs = LOGIN_TIME_OUT_CHECK;
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void LoginHeartBeatTimerEvent::Execute() noexcept
{
	CChatServer *chatServer = (CChatServer *)g_NetServer;
	chatServer->LoginHeartBeat();
}
