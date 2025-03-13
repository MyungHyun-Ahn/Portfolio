#include "pch.h"
#include "CommonProtocol.h"
#include "ServerSetting.h"
#include "CNetServer.h"
#include "CLanServer.h"
#include "DBSetting.h"
#include "CDBConnector.h"
#include "CNetMonitoringServer.h"
#include "CLanMonitoringServer.h"
#include "MonitoringServerSetting.h"

#pragma warning(disable : 4101)

int main()
{
	CCrashDump crashDump;

	{
		CMyFileLoader serverConfigLoader;
		serverConfigLoader.Parse(L"ServerConfig.conf");

		// Server Setting Parse
		serverConfigLoader.Load(L"NetMonitorServer", L"IP", &NET_SETTING::openIP);
		serverConfigLoader.Load(L"NetMonitorServer", L"PORT", &NET_SETTING::openPort);
		serverConfigLoader.Load(L"NetMonitorServer", L"PACKET_KEY", &CEncryption::PACKET_KEY);
		serverConfigLoader.Load(L"NetMonitorServer", L"PACKET_CODE", &NET_SETTING::PACKET_CODE);
		serverConfigLoader.Load(L"NetMonitorServer", L"IOCP_WORKER_THREAD", &NET_SETTING::IOCP_WORKER_THREAD);
		serverConfigLoader.Load(L"NetMonitorServer", L"IOCP_ACTIVE_THREAD", &NET_SETTING::IOCP_ACTIVE_THREAD);
		serverConfigLoader.Load(L"NetMonitorServer", L"USE_ZERO_COPY", &NET_SETTING::USE_ZERO_COPY);
		serverConfigLoader.Load(L"NetMonitorServer", L"MAX_SESSION_COUNT", &NET_SETTING::MAX_SESSION_COUNT);
		serverConfigLoader.Load(L"NetMonitorServer", L"ACCEPTEX_COUNT", &NET_SETTING::ACCEPTEX_COUNT);
		serverConfigLoader.Load(L"NetMonitorServer", L"LOGIN_KEY", &MONITOR_SETTING::LOGIN_KEY);

		serverConfigLoader.Load(L"LanMonitorServer", L"IP", &LAN_SETTING::openIP);
		serverConfigLoader.Load(L"LanMonitorServer", L"PORT", &LAN_SETTING::openPort);
		serverConfigLoader.Load(L"LanMonitorServer", L"IOCP_WORKER_THREAD", &LAN_SETTING::IOCP_WORKER_THREAD);
		serverConfigLoader.Load(L"LanMonitorServer", L"IOCP_ACTIVE_THREAD", &LAN_SETTING::IOCP_ACTIVE_THREAD);
		serverConfigLoader.Load(L"LanMonitorServer", L"USE_ZERO_COPY", &LAN_SETTING::USE_ZERO_COPY);
		serverConfigLoader.Load(L"LanMonitorServer", L"MAX_SESSION_COUNT", &LAN_SETTING::MAX_SESSION_COUNT);
		serverConfigLoader.Load(L"LanMonitorServer", L"ACCEPTEX_COUNT", &LAN_SETTING::ACCEPTEX_COUNT);


		serverConfigLoader.Load(L"LanMonitorServer", L"CHAT_SERVER_NO", &MONITOR_SETTING::CHAT_SERVER_NO);
		serverConfigLoader.Load(L"LanMonitorServer", L"GAME_SERVER_NO", &MONITOR_SETTING::GAME_SERVER_NO);
		serverConfigLoader.Load(L"LanMonitorServer", L"LOGIN_SERVER_NO", &MONITOR_SETTING::LOGIN_SERVER_NO);

		// DB Setting Parse
		serverConfigLoader.Load(L"MySQL", L"IP", &MYSQL_SETTING::IP);
		serverConfigLoader.Load(L"MySQL", L"PORT", &MYSQL_SETTING::PORT);
		serverConfigLoader.Load(L"MySQL", L"ID", &MYSQL_SETTING::ID);
		serverConfigLoader.Load(L"MySQL", L"PASSWORD", &MYSQL_SETTING::PASSWORD);
		serverConfigLoader.Load(L"MySQL", L"DEFAULT_SCHEMA", &MYSQL_SETTING::DEFAULT_SCHEMA);

	}
	
	// mysql 라이브러리 멀티스레드 초기화
	mysql_library_init(0, nullptr, nullptr);

	g_Logger = CLogger::GetInstance();
	g_Logger->SetMainDirectory(L"LogFile");
	g_Logger->SetLogLevel(LOG_LEVEL::DEBUG);

	g_ProfileMgr = CProfileManager::GetInstance();

	LAN_SERVER::g_LanServer = new CLanMonitoringServer;
	LAN_SERVER::g_LanServer->Start(LAN_SETTING::openIP.c_str(), LAN_SETTING::openPort);

	NET_SERVER::g_NetServer = new CNetMonitoringServer;
	NET_SERVER::g_NetServer->Start(NET_SETTING::openIP.c_str(), NET_SETTING::openPort);

	LAN_SERVER::g_LanServer->WaitLanServerStop();
	NET_SERVER::g_NetServer->WaitNetServerStop();
	return 0;
}