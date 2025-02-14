#include "pch.h"
#include "BaseEvent.h"
#include "CNetServer.h"
#include "CNetSession.h"
#include "ChatSetting.h"
#include "CPlayer.h"
#include "CSector.h"
#include "CChatServer.h"
#include "ContentEvent.h"

void SectorBroadcastEvent::SetEvent() noexcept
{
	execute = std::bind(&SectorBroadcastEvent::Execute, this);
	timeMs = FRAME_PER_TICK;
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void SectorBroadcastEvent::Execute() noexcept
{
	CChatServer *chatServer = (CChatServer *)g_NetServer;
	chatServer->SectorBroadcast();
}

void NonLoginHeartBeatEvent::SetEvent() noexcept
{
	execute = std::bind(&NonLoginHeartBeatEvent::Execute, this);
	timeMs = NON_LOGIN_TIME_OUT_CHECK;
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void NonLoginHeartBeatEvent::Execute() noexcept
{
	CChatServer *chatServer = (CChatServer *)g_NetServer;
	chatServer->NonLoginHeartBeat();
}

void LoginHeartBeatEvent::SetEvent() noexcept
{
	execute = std::bind(&LoginHeartBeatEvent::Execute, this);
	timeMs = LOGIN_TIME_OUT_CHECK;
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void LoginHeartBeatEvent::Execute() noexcept
{
	CChatServer *chatServer = (CChatServer *)g_NetServer;
	chatServer->LoginHeartBeat();
}
