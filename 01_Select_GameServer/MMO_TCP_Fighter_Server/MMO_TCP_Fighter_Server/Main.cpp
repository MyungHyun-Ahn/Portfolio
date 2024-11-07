#include "pch.h"
#include "Config.h"
#include "DefinePacket.h"
#include "CSession.h"
#include "CServerCore.h"
#include "CSector.h"
#include "CPlayer.h"
#include "CGameServer.h"

BOOL monitorThreadRunning = TRUE;
CGameServer *g_pServer;

unsigned int MonitoringThreadFunc(LPVOID lpParam)
{
	DWORD mTime = timeGetTime();
	while (monitorThreadRunning)
	{
		// 처음 실행 때문에 1초 늦게 시작
		DWORD dTime = timeGetTime() - mTime;
		if (dTime < 1000)
			Sleep(1000 - dTime);

		g_monitor.Update(g_pServer->GetSessionCount(), g_pServer->GetPlayerCount());

		mTime += 1000;
	}

	return 0;
}

// InitializeSingleton
void InitializeSingleton()
{
	CCrashDump initCrashDump;
	g_Logger = CLogger::GetInstance();
	g_Logger->SetDirectory(L"LogFile");
	g_Logger->SetLogLevel(LOG_LEVEL::DEBUG);
	g_Profiler = CProfileManager::GetInstance();
}



int main()
{
	timeBeginPeriod(1);
	srand((unsigned int)time(nullptr));
	InitializeSingleton();

	g_pServer = new CGameServer;
	g_pServer->Start(SERVER_IP, SERVER_PORT, 10000);

	_beginthreadex(nullptr, NULL, MonitoringThreadFunc, nullptr, 0, nullptr);

	int prevTick = timeGetTime();
	while (true)
	{
		InterlockedIncrement(&g_monitor.m_lLoopCount);
		g_pServer->Select();

		// 게임 프레임 체크
		if (int time = timeGetTime(); time - prevTick >= TICK_PER_FRAME)
		{
			g_pServer->Update();
			prevTick += TICK_PER_FRAME;
			InterlockedIncrement(&g_monitor.m_lFPS);
		}

		g_pServer->ReleaseSession();
	}



	timeEndPeriod(1);
}