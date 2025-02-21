#include "pch.h"
#include "ClientSetting.h"
#include "CNetClient.h"
#include "LoginSetting.h"
#include "CLoginClient.h"

#pragma warning(disable : 4101)

int main()
{
	CCrashDump crashDump;

	{
		CMyFileLoader clientConfigLoader;
		clientConfigLoader.Parse(L"ClientConfig.conf");

		// Client Setting Parse
		clientConfigLoader.Load(L"Client", L"PACKET_KEY", &CEncryption::PACKET_KEY);
		clientConfigLoader.Load(L"Client", L"IOCP_WORKER_THREAD", &CLIENT_SETTING::IOCP_WORKER_THREAD);
		clientConfigLoader.Load(L"Client", L"IOCP_ACTIVE_THREAD", &CLIENT_SETTING::IOCP_ACTIVE_THREAD);
		clientConfigLoader.Load(L"Client", L"USE_ZERO_COPY", &CLIENT_SETTING::USE_ZERO_COPY);


		clientConfigLoader.Load(L"LoginServer", L"IP", &LOGIN_SETTING::IP);
		clientConfigLoader.Load(L"LoginServer", L"PORT", &LOGIN_SETTING::PORT);
		clientConfigLoader.Load(L"LoginServer", L"MAX_SESSION_COUNT", &LOGIN_SETTING::MAX_SESSION_COUNT);
		clientConfigLoader.Load(L"LoginServer", L"ACCOUNTNO", &LOGIN_SETTING::ACCOUNTNO);
		clientConfigLoader.Load(L"LoginServer", L"ID", &LOGIN_SETTING::ID);
		clientConfigLoader.Load(L"LoginServer", L"NICKNAME", &LOGIN_SETTING::NICKNAME);
		
	}

	g_Logger = CLogger::GetInstance();
	g_Logger->SetMainDirectory(L"LogFile");
	g_Logger->SetLogLevel(LOG_LEVEL::DEBUG);

	g_ProfileMgr = CProfileManager::GetInstance();

	NETWORK_CLIENT::g_netClientMgr = new NETWORK_CLIENT::CNetClientManager;
	NETWORK_CLIENT::g_netClientMgr->Init();

	LOGIN_CLIENT::g_LoginClient = new LOGIN_CLIENT::CLoginClient;
	LOGIN_CLIENT::g_LoginClient->Init(LOGIN_SETTING::MAX_SESSION_COUNT);

	NETWORK_CLIENT::g_netClientMgr->RegisterClient((NETWORK_CLIENT::CNetClient *)LOGIN_CLIENT::g_LoginClient);
	NETWORK_CLIENT::g_netClientMgr->Start();

	LOGIN_CLIENT::g_LoginClient->PostConnectEx(LOGIN_SETTING::IP.c_str(), LOGIN_SETTING::PORT);

	NETWORK_CLIENT::g_netClientMgr->Wait();

	return 0;
}