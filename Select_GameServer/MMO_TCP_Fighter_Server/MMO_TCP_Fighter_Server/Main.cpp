#include "pch.h"
#include "Config.h"
#include "DefinePacket.h"
#include "CSession.h"
#include "CServerCore.h"
#include "CPlayer.h"
#include "CGameServer.h"

// InitializeSingleton
void InitializeSingleton()
{
	CCrashDump initCrashDump;
	g_Logger = CLogger::GetInstance();
	g_Profiler = CProfileManager::GetInstance();
}

int main()
{
	timeBeginPeriod(1);
	srand((unsigned int)time(nullptr));
	InitializeSingleton();

	CServerCore *pServer = new CGameServer;
	pServer->Start(SERVER_IP, SERVER_PORT, 10000);

	while (true)
	{
		pServer->Select();

		// 게임 프레임 체크가 들어갈 것

		pServer->TimeoutCheck();
		pServer->Disconnect();
	}



	timeEndPeriod(1);
}