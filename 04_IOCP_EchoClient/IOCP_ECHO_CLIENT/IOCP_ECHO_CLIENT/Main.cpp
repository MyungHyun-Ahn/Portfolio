#include "pch.h"
#include "DefineNetwork.h"
#include "CSession.h"
#include "CLanClients.h"
#include "CEchoClients.h"

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

		CEchoClients *clients = (CEchoClients *)g_Clients;
		AcquireSRWLockExclusive(&clients->m_srwLock);

		INT nowTime = timeGetTime();
		LONG echoNotRecv = 0;
		LONG loginPacketNotRecv = 0;

		for (auto cl : clients->m_clientMap)
		{
			CClient *pClient = cl.second;
			AcquireSRWLockExclusive(&pClient->m_srwLock);
			if (nowTime - pClient->m_iPrevSendTime < 500)
			{
				echoNotRecv += (pClient->m_iSendNum - pClient->m_iRecvNum);
			}

			if (!pClient->m_bLoginPacketRecv && nowTime - pClient->m_iCreateTime < 3000)
				loginPacketNotRecv++;
			ReleaseSRWLockExclusive(&pClient->m_srwLock);
		}

		ReleaseSRWLockExclusive(&clients->m_srwLock);

		g_monitor.m_lEchoNotRecv = echoNotRecv;
		g_monitor.m_lLoginPacketNotRecv = loginPacketNotRecv;

		g_monitor.Update(g_Clients->GetSessionCount(), NULL);

		mTime += 1000;
	}

	return 0;
}

int main()
{
	std::string openIP;
	USHORT openPort;
	USHORT sessionCount;

	{
		CMyFileLoader serverConfigLoader;
		serverConfigLoader.Parse(L"Config.conf");

		serverConfigLoader.Load(L"Server", L"IP", &openIP);
		serverConfigLoader.Load(L"Server", L"PORT", &openPort);
		serverConfigLoader.Load(L"Server", L"SESSION_COUNT", &sessionCount);
		serverConfigLoader.Load(L"Server", L"OVERSEND_COUNT", &oversendCount);
	}

	g_Logger = CLogger::GetInstance();
	g_Logger->SetDirectory(L"LogFile");
	g_Logger->SetLogLevel(LOG_LEVEL::DEBUG);

	CCrashDump crashDump;
	g_Clients = new CEchoClients;
	g_Clients->Start(openIP.c_str(), openPort, 16, 4, sessionCount);

	MonitorThreadFunc(nullptr);

	return 0;
}