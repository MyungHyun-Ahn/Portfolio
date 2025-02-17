#include "pch.h"
#include "ServerSetting.h"
#include "CNetServer.h"
#include "CNetSession.h"
#include "DBSetting.h"
#include "CDBConnector.h"
#include "CUser.h"
#include "LoginServerSetting.h"
#include "CLoginServer.h"

#pragma warning(disable : 4101)

int main()
{
	CCrashDump crashDump;

	{
		CMyFileLoader serverConfigLoader;
		serverConfigLoader.Parse(L"ServerConfig.conf");

		// Server Setting Parse
		serverConfigLoader.Load(L"Server", L"IP", &SERVER_SETTING::openIP);
		serverConfigLoader.Load(L"Server", L"PORT", &SERVER_SETTING::openPort);
		serverConfigLoader.Load(L"Server", L"PACKET_KEY", &CEncryption::PACKET_KEY);
		serverConfigLoader.Load(L"Server", L"IOCP_WORKER_THREAD", &SERVER_SETTING::IOCP_WORKER_THREAD);
		serverConfigLoader.Load(L"Server", L"IOCP_ACTIVE_THREAD", &SERVER_SETTING::IOCP_ACTIVE_THREAD);
		serverConfigLoader.Load(L"Server", L"USE_ZERO_COPY", &SERVER_SETTING::USE_ZERO_COPY);
		serverConfigLoader.Load(L"Server", L"MAX_SESSION_COUNT", &SERVER_SETTING::MAX_SESSION_COUNT);
		serverConfigLoader.Load(L"Server", L"ACCEPTEX_COUNT", &SERVER_SETTING::ACCEPTEX_COUNT);

		// Login Server Setting Parse
		serverConfigLoader.Load(L"LoginServer", L"LOGIN_TIME_OUT", &LOGIN_SERVER_SETTING::LOGIN_TIME_OUT);
		serverConfigLoader.Load(L"LoginServer", L"CHAT_SERVER_IP_1", &LOGIN_SERVER_SETTING::CHAT_SERVER_IP_1);
		serverConfigLoader.Load(L"LoginServer", L"CHAT_SERVER_PORT_1", &LOGIN_SERVER_SETTING::CHAT_SERVER_PORT_1);

		// DB Setting Parse
		serverConfigLoader.Load(L"MySQL", L"IP", &MYSQL_SETTING::IP);
		serverConfigLoader.Load(L"MySQL", L"PORT", &MYSQL_SETTING::PORT);
		serverConfigLoader.Load(L"MySQL", L"ID", &MYSQL_SETTING::ID);
		serverConfigLoader.Load(L"MySQL", L"PASSWORD", &MYSQL_SETTING::PASSWORD);
		serverConfigLoader.Load(L"MySQL", L"DEFAULT_SCHEMA", &MYSQL_SETTING::DEFAULT_SCHEMA);

		serverConfigLoader.Load(L"Redis", L"IP", &REDIS_SETTING::IP);
		serverConfigLoader.Load(L"Redis", L"PORT", &REDIS_SETTING::PORT);
	}
	
	// mysql 라이브러리 멀티스레드 초기화
	mysql_library_init(0, nullptr, nullptr);

	g_Logger = CLogger::GetInstance();
	g_Logger->SetMainDirectory(L"LogFile");
	g_Logger->SetLogLevel(LOG_LEVEL::DEBUG);

	g_ProfileMgr = CProfileManager::GetInstance();

	g_NetServer = new CLoginServer;

	g_NetServer->Start(SERVER_SETTING::openIP.c_str(), SERVER_SETTING::openPort);

	return 0;
}