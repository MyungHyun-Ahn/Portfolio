#include "pch.h"
#include "ServerSetting.h"
#include "CNetServer.h"
#include "CNetSession.h"
#include "CPlayer.h"
#include "CSector.h"
#include "ChatSetting.h"
#include "CChatServer.h"

#pragma warning(disable : 4101)

int main()
{
	CCrashDump crashDump;

	{
		CMyFileLoader serverConfigLoader;
		serverConfigLoader.Parse(L"ServerConfig.conf");

		serverConfigLoader.Load(L"Server", L"IP", &openIP);
		serverConfigLoader.Load(L"Server", L"PORT", &openPort);
		serverConfigLoader.Load(L"Server", L"PACKET_KEY", &CEncryption::PACKET_KEY);
		serverConfigLoader.Load(L"Server", L"IOCP_WORKER_THREAD", &IOCP_WORKER_THREAD);
		serverConfigLoader.Load(L"Server", L"IOCP_ACTIVE_THREAD", &IOCP_ACTIVE_THREAD);
		serverConfigLoader.Load(L"Server", L"USE_ZERO_COPY", &USE_ZERO_COPY);
		serverConfigLoader.Load(L"Server", L"MAX_SESSION_COUNT", &MAX_SESSION_COUNT);
		serverConfigLoader.Load(L"Server", L"ACCEPTEX_COUNT", &ACCEPTEX_COUNT);

		serverConfigLoader.Load(L"ChatSetting", L"SERVER_FPS", &FPS);
		if (FPS == 0)
			FRAME_PER_TICK = 0;
		else
			FRAME_PER_TICK = 1000 / FPS;
		serverConfigLoader.Load(L"ChatSetting", L"NON_LOGIN_TIME_OUT", &NON_LOGIN_TIME_OUT);
		NON_LOGIN_TIME_OUT_CHECK = NON_LOGIN_TIME_OUT;
		serverConfigLoader.Load(L"ChatSetting", L"LOGIN_TIME_OUT", &LOGIN_TIME_OUT);
		LOGIN_TIME_OUT_CHECK = LOGIN_TIME_OUT;
	}
	
	g_Logger = CLogger::GetInstance();
	g_Logger->SetMainDirectory(L"LogFile");
	g_Logger->SetLogLevel(LOG_LEVEL::DEBUG);

	g_ProfileMgr = CProfileManager::GetInstance();

	g_NetServer = new CChatServer;

	g_NetServer->Start(openIP.c_str(), openPort);

	return 0;
}