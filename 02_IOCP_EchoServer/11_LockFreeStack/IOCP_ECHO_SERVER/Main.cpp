#include "pch.h"
#include "CLanServer.h"
#include "CSession.h"
#include "CEchoServer.h"

BOOL monitorThreadRunning = TRUE;

unsigned int MonitorThreadFunc(LPVOID lpParam)
{
	DWORD mTime = timeGetTime();
	while (monitorThreadRunning)
	{
		// ó�� ���� ������ 1�� �ʰ� ����
		DWORD dTime = timeGetTime() - mTime;
		if (dTime < 1000)
			Sleep(1000 - dTime);

		g_monitor.Update(g_Server->GetSessionCount(), NULL);

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
	}

	g_Logger = CLogger::GetInstance();
	g_Logger->SetDirectory(L"LogFile");
	g_Logger->SetLogLevel(LOG_LEVEL::DEBUG);

	CCrashDump crashDump;
	g_Server = new CEchoServer;
	g_Server->Start(openIP.c_str(), openPort, 16, 4, 65535);

	MonitorThreadFunc(nullptr);

	return 0;
}