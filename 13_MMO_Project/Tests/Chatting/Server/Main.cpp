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
#include "CMetricsExporter.h"
#include "ChatMetrics.h"

int main(void)
{
	using MHLib::utils::ELogLevel;
	using MHLib::utils::g_pLogger;
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	g_pLogger = MHLib::utils::CLogger::GetInstance();
	Chatting::SChatConfig config;
	if (!Chatting::LoadConfig(L"ServerConfig.conf", true, config))
		return 2;
	if (!g_pLogger->Start(L"LogFile", config.m_consoleLogLevel, config.m_fileLogLevel,
		config.m_logPollInterval, static_cast<std::size_t>(config.m_logQueueCapacity)))
	{
		g_pLogger->WriteLogConsole(ELogLevel::Error, L"CLogger: cannot open the async log file");
		return 2;
	}
	g_pLogger->RegisterLogProducer();

	MetricsLib::CMetricsExporter metricsExporter;
	if (config.m_metricsPort != 0)
	{
		uint16_t boundPort = 0;
		if (metricsExporter.Start(config.m_metricsBindAddress, config.m_metricsPort, boundPort))
		{
			g_pLogger->SetPeriodicCallback(metricsExporter.CreatePublishCallback(Chatting::MakeChatMetricsCollector()));
			g_pLogger->WriteLog(ELogLevel::Info, L"ChatServer", 0, std::source_location::current(),
				L"event=metrics_started port=%u | 메트릭 리스너를 시작했습니다.", static_cast<unsigned>(boundPort));
		}
		else
		{
			g_pLogger->WriteLog(ELogLevel::Error, L"ChatServer", 0, std::source_location::current(),
				L"event=metrics_start_failed port=%u | 메트릭 리스너를 시작하지 못했습니다. 채팅 서버는 계속 시작합니다.",
				static_cast<unsigned>(config.m_metricsPort));
		}
	}
	const auto stopMetrics = [&metricsExporter]
	{
		g_pLogger->SetPeriodicCallback({});
		metricsExporter.Stop();
	};

	ServerNetworkLib::serverSetting::openIp = config.m_ip;
	ServerNetworkLib::serverSetting::openPort = config.m_port;
	ServerNetworkLib::serverSetting::maxSessionCount = config.m_maxSessionCount;
	ServerNetworkLib::serverSetting::acceptexCount = config.m_acceptExCount;
	ServerNetworkLib::serverSetting::iocpWorkerThread = config.m_iocpWorkerCount;
	ServerNetworkLib::serverSetting::iocpActiveThread = config.m_iocpConcurrency;
	ServerNetworkLib::serverSetting::contentsThreadCount = config.m_contentsWorkerCount;
	ServerNetworkLib::serverSetting::maxContentsFps = config.m_contentsFps;
	ServerNetworkLib::serverSetting::packetCode = Chatting::kPacketCode;
	ServerNetworkLib::serverSetting::packetKey = Chatting::kPacketKey;
	MHLib::security::CEncryption::sm_packetKey = Chatting::kPacketKey;
	MHLib::utils::g_pProfileManager = MHLib::utils::CProfileManager::GetInstance();
	g_pLogger->WriteLog(ELogLevel::Info, L"ChatServer", 0, std::source_location::current(),
		L"event=server_config acceptex_count=%d iocp_worker_count=%d iocp_concurrency=%d contents_worker_count=%d log_queue_capacity=%d"
		L" | 서버 시작 설정을 적용했습니다.", config.m_acceptExCount, config.m_iocpWorkerCount, config.m_iocpConcurrency,
		config.m_contentsWorkerCount, config.m_logQueueCapacity);

	auto *pServer = new Chatting::CChatServer;
	ServerNetworkLib::g_pNetServer = pServer;
	std::atomic<bool> isReturned = false;
	std::atomic<bool> isStarted = false;
	std::thread serverThread([&]()
	{
		g_pLogger->RegisterLogProducer();
		isStarted = pServer->Start(config.m_ip.c_str(), config.m_port) != FALSE;
		isReturned = true;
	});

	std::string pending;
	WCHAR highSurrogate = 0;
	while (!isReturned)
	{
		std::string line;
		auto result = Chatting::PollInput(pending, highSurrogate, line);
		if (result == Chatting::EInputResult::End || (result == Chatting::EInputResult::Line && line == "/exit"))
		{
			g_pLogger->WriteLog(ELogLevel::Info, L"ChatServer", 0, std::source_location::current(),
				L"event=server_process_exit | 서버 프로세스를 종료합니다. NetworkLib 정상 종료 처리는 보류 중입니다.");
			stopMetrics();
			fflush(nullptr);
			ExitProcess(0);
		}
		Sleep(10);
	}
	serverThread.join();
	const bool didStart = isStarted.load();
	if (didStart)
	{
		g_pLogger->WriteLog(ELogLevel::Info, L"ChatServer", 0, std::source_location::current(),
			L"event=server_run_ended | 서버 실행이 종료되었습니다.");
	}
	else
	{
		g_pLogger->WriteLog(ELogLevel::Error, L"ChatServer", 0, std::source_location::current(),
			L"event=server_start_failed | 서버 시작에 실패했습니다.");
	}
	// 생산자가 남을 수 있는 부분 시작 실패와 /exit의 수명은 이 단계에서 바꾸지 않는다.
	stopMetrics();
	if (!pServer->HasStartedBackgroundWork())
		g_pLogger->Stop();
	fflush(nullptr);
	ExitProcess(didStart ? 0 : 1);
}
