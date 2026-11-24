#include "pch.h"
#include <memory>
#include <condition_variable>
#include "CSerializableBuffer.h"
#include "CommonProtocol.h"
#include "Config.h"
#include "Console.h"
#include "CClientManager.h"
#include "CClient.h"
#include "ClientPacketDispatch.g.h"
#include "CChatNetClient.g.h"
#include "CConsoleChatClient.h"

int main(void)
{
	using MHLib::utils::ELogLevel;
	using MHLib::utils::g_pLogger;
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	g_pLogger = MHLib::utils::CLogger::GetInstance();
	g_pLogger->SetMainDirectory(L"LogFile");
	Chatting::SChatConfig config;
	if (!Chatting::LoadConfig(L"ClientConfig.conf", false, config))
		return 2;

	int exitCode = 0;
	{
		ClientNetworkLib::CClientManager network;
		Chatting::CConsoleChatClient client(network, config);
		MHLib::security::CEncryption::sm_packetKey = Chatting::kPacketKey;
		if (network.Init(1, config.m_recvBufferBytes, config.m_packetMode)
			&& client.Connect(config.m_ip.c_str(), config.m_port, Chatting::kPacketCode, config.m_connectTimeoutMilliseconds))
		{
			while (client.IsActive() && !client.IsConnected())
			{
				client.Tick();
				Sleep(10);
			}
		}
		if (!client.IsConnected())
		{
			g_pLogger->WriteLogConsole(ELogLevel::Error, L"CONNECT FAILED ip=%hs port=%u", config.m_ip.c_str(), config.m_port);
			exitCode = 1;
		}
		else
		{
			DWORD startTime = timeGetTime();
			while (client.IsConnected() && !client.IsChatReady() && timeGetTime() - startTime < 5000)
			{
				client.Tick();
				Sleep(10);
			}
			if (!client.IsChatReady())
			{
				g_pLogger->WriteLogConsole(ELogLevel::Error, L"LOGIN/SECTOR FAILED");
				exitCode = 1;
			}
			else
			{
				g_pLogger->WriteLogConsole(ELogLevel::Info, L"Type a message, /exit to quit. Test-only authentication.");
				std::string pending;
				WCHAR highSurrogate = 0;
				DWORD prevHeartbeatTime = timeGetTime();
				bool isUserExit = false;
				while (client.IsConnected())
				{
					client.Tick();
					if (!client.IsConnected())
						break;
					std::string line;
					auto inputResult = Chatting::PollInput(pending, highSurrogate, line);
					if (inputResult == Chatting::EInputResult::End || (inputResult == Chatting::EInputResult::Line && line == "/exit"))
					{
						isUserExit = true;
						break;
					}
					if (inputResult == Chatting::EInputResult::Line && !line.empty())
					{
						std::wstring text;
						if (!client.IsChatReady() || !Chatting::Utf8ToWide(line, text) || text.empty()
							|| text.size() > Chatting::kMaxMessageBytes / sizeof(WCHAR)
							|| !client.SendReqMessage(config.m_accountNo, text))
						{
							g_pLogger->WriteLogConsole(ELogLevel::Error, L"MESSAGE REJECTED: use UTF-8, 1..256 UTF-16 code units");
						}
					}
					if (timeGetTime() - prevHeartbeatTime >= Chatting::kHeartbeatMilliseconds)
					{
						if (client.IsChatReady())
							(void)client.SendNotiHeartbeat(config.m_accountNo);
						prevHeartbeatTime = timeGetTime();
					}
					Sleep(10);
				}
				if (!isUserExit)
					exitCode = 1;
			}
		}
		client.Disconnect();
		network.Stop();
		client.Tick();
	}
	fflush(nullptr);
	// 클라이언트 IO는 위에서 정리했다. 상속된 정적 풀 종료는 별도 범위다.
	ExitProcess(exitCode);
}
