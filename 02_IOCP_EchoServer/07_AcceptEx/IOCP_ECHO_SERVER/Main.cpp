#include "pch.h"
#include "ServerConfig.h"
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
	g_Logger = CLogger::GetInstance();
	g_Logger->SetDirectory(L"LogFile");
	g_Logger->SetLogLevel(LOG_LEVEL::DEBUG);
		
	CCrashDump crashDump;
	g_Server = new CEchoServer;
	g_Server->Start(SERVER_IP, SERVER_PORT, 4, 4, 100);

	MonitorThreadFunc(nullptr);

	return 0;
}