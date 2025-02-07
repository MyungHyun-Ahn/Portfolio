#include "pch.h"
#include "ServerSetting.h"
#include "CLanServer.h"
#include "CLanSession.h"
#include "CEchoServer.h"

int main()
{
	std::string openIP;
	USHORT openPort;

	{
		CMyFileLoader serverConfigLoader;
		serverConfigLoader.Parse(L"ServerConfig.conf");

		serverConfigLoader.Load(L"Server", L"IP", &openIP);
		serverConfigLoader.Load(L"Server", L"PORT", &openPort);
		serverConfigLoader.Load(L"Server", L"FIXED_KEY", &CEncryption::FIXED_KEY);

		serverConfigLoader.Load(L"Server", L"IOCP_WORKER_THREAD", &IOCP_WORKER_THREAD);
		serverConfigLoader.Load(L"Server", L"IOCP_ACTIVE_THREAD", &IOCP_ACTIVE_THREAD);
		serverConfigLoader.Load(L"Server", L"USE_ZERO_COPY", &USE_ZERO_COPY);
		serverConfigLoader.Load(L"Server", L"MAX_SESSION_COUNT", &MAX_SESSION_COUNT);
		serverConfigLoader.Load(L"Server", L"ACCEPTEX_COUNT", &ACCEPTEX_COUNT);
	}
	
	g_Logger = CLogger::GetInstance();
	g_Logger->SetMainDirectory(L"LogFile");
	g_Logger->SetLogLevel(LOG_LEVEL::DEBUG);

	g_ProfileMgr = CProfileManager::GetInstance();

	CCrashDump crashDump;
	g_LanServer = new CEchoServer;
	g_LanServer->Start(openIP.c_str(), openPort);

	return 0;
}