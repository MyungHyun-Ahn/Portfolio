#include "pch.h"
#include "BaseEvent.h"
#include "CNetServer.h"
#include "CNetSession.h"
#include "CUser.h"
#include "CLoginServer.h"
#include "ContentEvent.h"
#include "LoginServerSetting.h"

void HeartBeatEvent::SetEvent() noexcept
{
	execute = std::bind(&HeartBeatEvent::Execute, this);
	timeMs = LOGIN_SERVER_SETTING::LOGIN_TIME_OUT;
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void HeartBeatEvent::Execute() noexcept
{
	CLoginServer *chatServer = (CLoginServer *)g_NetServer;
	chatServer->HeartBeat();
}