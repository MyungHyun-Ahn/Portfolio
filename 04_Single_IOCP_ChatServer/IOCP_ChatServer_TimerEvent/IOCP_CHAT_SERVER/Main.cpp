#include "pch.h"
#include "ServerSetting.h"
#include "CNetServer.h"
#include "CNetSession.h"
#include "CPlayer.h"
#include "CSector.h"
#include "ChatSetting.h"
#include "CChatServer.h"

#pragma warning(disable : 4101)
#pragma warning(disable : 6011)

// 유저 메모리 부하 테스트
#define GB (1024LL * 1024 * 1024) // 1GB
#define PAGE_SIZE 4096            // 4KB

BYTE *dummyPtr[(8 * GB) / (16 * PAGE_SIZE)];

unsigned int TestThreadFunc(LPVOID lpParam) noexcept
{
	int err;
	// 8GB
	for (size_t i = 0; i < (8 * GB) / (16 * PAGE_SIZE); i++)
	{
		dummyPtr[i] = (BYTE *)VirtualAlloc(nullptr, (16 * PAGE_SIZE), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	}
	DWORD mTime = timeGetTime();
	while (TRUE)
	{

		for (size_t i = 0; i < (8 * GB) / (16 * PAGE_SIZE); i++)
		{
			for (size_t offset = 0; offset < (16 * PAGE_SIZE); offset += PAGE_SIZE)
			{
				BYTE *curPage = dummyPtr[i] + offset;
				BYTE value = *curPage;
				// 참조를 통한 페이지 인 유도
				(*curPage)++;
			}
		}
	}
}


int main()
{
	CCrashDump crashDump;

	{
		CMyFileLoader serverConfigLoader;
		serverConfigLoader.Parse(L"ServerConfig.conf");

		serverConfigLoader.Load(L"Server", L"IP", &openIP);
		serverConfigLoader.Load(L"Server", L"PORT", &openPort);
		serverConfigLoader.Load(L"Server", L"PACKET_KEY", &CEncryption::PACKET_KEY);
		serverConfigLoader.Load(L"Server", L"IOCP_WORKER_THREAD", &IOCP_WORKER_THREAD);
		serverConfigLoader.Load(L"Server", L"IOCP_ACTIVE_THREAD", &IOCP_ACTIVE_THREAD);
		serverConfigLoader.Load(L"Server", L"USE_ZERO_COPY", &USE_ZERO_COPY);
		serverConfigLoader.Load(L"Server", L"MAX_SESSION_COUNT", &MAX_SESSION_COUNT);
		serverConfigLoader.Load(L"Server", L"ACCEPTEX_COUNT", &ACCEPTEX_COUNT);

		serverConfigLoader.Load(L"ChatSetting", L"SERVER_FPS", &FPS);
		if (FPS == 0)
			FRAME_PER_TICK = 0;
		else
			FRAME_PER_TICK = 1000 / FPS;
		serverConfigLoader.Load(L"ChatSetting", L"SECTOR_BROATCAST_TIMER_TICK", &SECTOR_BROATCAST_TIMER_TICK);
		serverConfigLoader.Load(L"ChatSetting", L"NON_LOGIN_TIME_OUT", &NON_LOGIN_TIME_OUT);
		NON_LOGIN_TIME_OUT_CHECK = NON_LOGIN_TIME_OUT;
		serverConfigLoader.Load(L"ChatSetting", L"LOGIN_TIME_OUT", &LOGIN_TIME_OUT);
		LOGIN_TIME_OUT_CHECK = LOGIN_TIME_OUT;
	}
	
	g_Logger = CLogger::GetInstance();
	g_Logger->SetMainDirectory(L"LogFile");
	g_Logger->SetLogLevel(LOG_LEVEL::DEBUG);

	g_ProfileMgr = CProfileManager::GetInstance();

	g_NetServer = new CChatServer;

	g_NetServer->Start(openIP.c_str(), openPort);

	// _beginthreadex(nullptr, 0, TestThreadFunc, nullptr, 0, nullptr);

	return 0;
}