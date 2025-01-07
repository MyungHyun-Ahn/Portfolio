#include "pch.h"
#include "CLanServer.h"
#include "CLanSession.h"
#include "CEchoServer.h"

BOOL monitorThreadRunning = TRUE;

unsigned int MonitorThreadFunc(LPVOID lpParam)
{
	DWORD mTime = timeGetTime();
	while (monitorThreadRunning)
	{
		// 처음 실행 때문에 1초 늦게 시작
		DWORD dTime = timeGetTime() - mTime;
		if (dTime < 1000)
			Sleep(1000 - dTime);

		g_monitor.Update(g_LanServer->GetSessionCount(), NULL);

		mTime += 1000;
	}

	return 0;
}

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
	}
	
	g_Logger = CLogger::GetInstance();
	g_Logger->SetMainDirectory(L"LogFile");
	g_Logger->SetLogLevel(LOG_LEVEL::DEBUG);

	g_ProfileMgr = CProfileManager::GetInstance();

	CCrashDump crashDump;
	g_LanServer = new CEchoServer;
	g_LanServer->Start(openIP.c_str(), openPort, 16, 4, 65535);

	MonitorThreadFunc(nullptr);

	return 0;
}