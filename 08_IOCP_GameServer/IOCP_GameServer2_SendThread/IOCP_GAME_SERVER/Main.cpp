#include "pch.h"
#include "ServerSetting.h"
#include "CNetServer.h"
#include "CGameServer.h"
#include "ClientSetting.h"
#include "MonitorSetting.h"
#include "CLanClient.h"
#include "CMonitorClient.h"

#pragma warning(disable : 4101)

int main()
{
	timeBeginPeriod(1);

	g_Logger = CLogger::GetInstance();
	g_Logger->SetMainDirectory(L"LogFile");
	g_Logger->SetLogLevel(LOG_LEVEL::DEBUG);
	g_ProfileMgr = CProfileManager::GetInstance();

	CCrashDump crashDump;

	{
		CMyFileLoader serverConfigLoader;
		serverConfigLoader.Parse(L"ServerConfig.conf");

		serverConfigLoader.Load(L"Server", L"IP", &SERVER_SETTING::openIP);
		serverConfigLoader.Load(L"Server", L"PORT", &SERVER_SETTING::openPort);
		serverConfigLoader.Load(L"Server", L"PACKET_KEY", &CEncryption::PACKET_KEY);
		serverConfigLoader.Load(L"Server", L"PACKET_CODE", &SERVER_SETTING::PACKET_CODE);
		serverConfigLoader.Load(L"Server", L"IOCP_WORKER_THREAD", &SERVER_SETTING::IOCP_WORKER_THREAD);
		serverConfigLoader.Load(L"Server", L"IOCP_ACTIVE_THREAD", &SERVER_SETTING::IOCP_ACTIVE_THREAD);
		serverConfigLoader.Load(L"Server", L"USE_ZERO_COPY", &SERVER_SETTING::USE_ZERO_COPY);
		serverConfigLoader.Load(L"Server", L"MAX_SESSION_COUNT", &SERVER_SETTING::MAX_SESSION_COUNT);
		serverConfigLoader.Load(L"Server", L"ACCEPTEX_COUNT", &SERVER_SETTING::ACCEPTEX_COUNT);

		serverConfigLoader.Load(L"Server", L"CONTENT_THREAD_COUNT", &SERVER_SETTING::CONTENT_THREAD_COUNT);
		serverConfigLoader.Load(L"Server", L"MAX_CONTENT_FPS", &SERVER_SETTING::MAX_CONTENT_FPS);
		serverConfigLoader.Load(L"Server", L"DELAY_FRAME", &SERVER_SETTING::DELAY_FRAME);

		serverConfigLoader.Load(L"Monitoring", L"USE_ZERO_COPY", &CLIENT_SETTING::USE_ZERO_COPY);
		serverConfigLoader.Load(L"Monitoring", L"IOCP_WORKER_THREAD", &CLIENT_SETTING::IOCP_WORKER_THREAD);
		serverConfigLoader.Load(L"Monitoring", L"IOCP_ACTIVE_THREAD", &CLIENT_SETTING::IOCP_ACTIVE_THREAD);
		serverConfigLoader.Load(L"Monitoring", L"SERVER_NO", &MONITOR_SETTING::SERVER_NO);
		serverConfigLoader.Load(L"Monitoring", L"IP", &MONITOR_SETTING::IP);
		serverConfigLoader.Load(L"Monitoring", L"PORT", &MONITOR_SETTING::PORT);

	}

	LAN_CLIENT::g_netClientMgr = new LAN_CLIENT::CLanClientManager;
	LAN_CLIENT::g_netClientMgr->Init();

	g_MonitorClient = new CMonitorClient;
	g_MonitorClient->Init(1); // 1°³ ¿¬°á
	LAN_CLIENT::g_netClientMgr->RegisterClient(g_MonitorClient);
	LAN_CLIENT::g_netClientMgr->Start();

	g_MonitorClient->PostConnectEx(MONITOR_SETTING::IP.c_str(), MONITOR_SETTING::PORT);


	NET_SERVER::g_NetServer = new CGameServer;
	NET_SERVER::g_NetServer->Start(SERVER_SETTING::openIP.c_str(), SERVER_SETTING::openPort);
	LAN_CLIENT::g_netClientMgr->Wait();

	return 0;
}