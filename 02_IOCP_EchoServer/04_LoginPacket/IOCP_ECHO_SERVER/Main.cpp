#include "pch.h"
#include "ServerConfig.h"
#include "CLanServer.h"
#include "CSession.h"
#include "CEchoServer.h"

int main()
{
	g_Logger = CLogger::GetInstance();
	g_Logger->SetDirectory(L"LogFile");
	g_Logger->SetLogLevel(LOG_LEVEL::DEBUG);
	g_Server = new CEchoServer;

	g_Server->Start(SERVER_IP, SERVER_PORT, 2, 2, 100);

	while (true)
	{
		YieldProcessor();
	}

	return 0;
}