#include "pch.h"
#include "NetworkLimits.h"
#include "COverlappedAllocator.h"
#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CRecvBuffer.h"
#include "CSerializableBufferView.h"
#include "ServerSetting.h"
#include "CNetServer.h"
#include "CommonProtocol.h"
#include "Config.h"
#include "Console.h"
#include "CChatServer.h"

int main(void)
{
	using MHLib::utils::ELogLevel;
	using MHLib::utils::g_pLogger;
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	g_pLogger = MHLib::utils::CLogger::GetInstance();
	g_pLogger->SetMainDirectory(L"LogFile");
	Chatting::SChatConfig config;
	if (!Chatting::LoadConfig(L"ServerConfig.conf", true, config))
		return 2;

	NetworkLib::serverSetting::openIp = config.m_ip;
	NetworkLib::serverSetting::openPort = config.m_port;
	NetworkLib::serverSetting::maxSessionCount = 256;
	NetworkLib::serverSetting::acceptexCount = 4;
	NetworkLib::serverSetting::iocpWorkerThread = 2;
	NetworkLib::serverSetting::iocpActiveThread = 2;
	NetworkLib::serverSetting::contentsThreadCount = 2;
	NetworkLib::serverSetting::packetCode = Chatting::kPacketCode;
	NetworkLib::serverSetting::packetKey = Chatting::kPacketKey;
	MHLib::security::CEncryption::sm_packetKey = Chatting::kPacketKey;
	MHLib::utils::g_pProfileManager = MHLib::utils::CProfileManager::GetInstance();

	auto *pServer = new Chatting::CChatServer;
	NetworkLib::g_pNetServer = pServer;
	std::atomic<bool> isReturned = false;
	std::atomic<bool> isStarted = false;
	std::thread serverThread([&]()
	{
		isStarted = pServer->Start(config.m_ip.c_str(), config.m_port) != FALSE;
		isReturned = true;
	});

	g_pLogger->WriteLogConsole(ELogLevel::System, L"LOCAL TEST ONLY. /exit or EOF ends the server process.");
	std::string pending;
	WCHAR highSurrogate = 0;
	while (!isReturned)
	{
		std::string line;
		auto result = Chatting::PollInput(pending, highSurrogate, line);
		if (result == Chatting::EInputResult::End || (result == Chatting::EInputResult::Line && line == "/exit"))
		{
			g_pLogger->WriteLogConsole(ELogLevel::System, L"STOP process boundary (NetworkLib Stop lifecycle is deferred)");
			fflush(nullptr);
			ExitProcess(0);
		}
		Sleep(10);
	}
	serverThread.join();
	g_pLogger->WriteLogConsole(ELogLevel::Error, L"SERVER START RETURNED success=%d", static_cast<int>(isStarted.load()));
	fflush(nullptr);
	ExitProcess(isStarted ? 0 : 1);
}
