#include "pch.h"
#include "ContentEvent.h"
#include "CNetClient.h"
#include "LoginSetting.h"
#include "CLoginClient.h"

void LoginEvent::SetEvent(UINT64 sessionId) noexcept
{
	execute = std::bind(&LoginEvent::Execute, this, sessionId);
}

void LoginEvent::Execute(UINT64 sessionId) noexcept
{
	LOGIN_CLIENT::g_LoginClient->LoginPacketReq(sessionId);
}
