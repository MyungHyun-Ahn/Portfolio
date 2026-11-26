#include "pch.h"
#include <pdh.h>
#include "NetworkLimits.h"
#include "COverlappedAllocator.h"
#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CRecvBuffer.h"
#include "CSerializableBufferView.h"
#include "CServer.h"
#include "SServerNetworkSettings.h"
#include "SContentsRuntimeSettings.h"
#include "MHLib/utils/SLoggingSettings.h"
#include "SMetricsSettings.h"
#include "CommonProtocol.h"
#include "ServerConfig.h"
#include "Console.h"
#include "IContents.h"
#include "ContentsHandle.h"
#include "CContentsRuntime.h"
#include "CChatServer.h"
#include "CMetricsExporter.h"
#include "CSystemMetrics.h"
#include "../../../Server/ServerMetrics/ServerMetrics.h"
#include "ChatMetrics.h"

int main(void)
{
	using MHLib::utils::ELogLevel;
	using MHLib::utils::g_pLogger;
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	g_pLogger = MHLib::utils::CLogger::GetInstance();
	ServerNetworkLib::SServerNetworkSettings networkSettings{};
	ContentsRuntimeLib::SContentsRuntimeSettings contentsConfig{};
	MHLib::utils::SLoggingSettings loggingConfig{};
	MetricsLib::SMetricsSettings metricsConfig{};
	if (!Chatting::LoadServerConfig(L"ServerConfig.conf", networkSettings, contentsConfig, loggingConfig, metricsConfig))
		return 2;
	if (!g_pLogger->Start(L"LogFile", loggingConfig.consoleLevel, loggingConfig.fileLevel,
		loggingConfig.pollInterval, loggingConfig.queueCapacity))
	{
		g_pLogger->WriteLogConsole(ELogLevel::Error, L"CLogger: cannot open the async log file");
		return 2;
	}
	g_pLogger->RegisterLogProducer();

	MetricsLib::CMetricsExporter metricsExporter;
	std::optional<MetricsLib::CSystemMetrics> systemMetrics;
	ServerMetrics::CServerMetrics serverMetrics;
	Chatting::CChatMetrics chatMetrics;
	if (metricsConfig.port != 0)
	{
		uint16_t boundPort = 0;
		if (metricsExporter.Start(metricsConfig.bindAddress, metricsConfig.port, boundPort))
		{
			systemMetrics.emplace();
			g_pLogger->WriteLog(ELogLevel::Info, L"ChatServer", 0, std::source_location::current(),
				L"event=metrics_started port=%u | 메트릭 리스너를 시작했습니다.", static_cast<unsigned>(boundPort));
		}
		else
		{
			g_pLogger->WriteLog(ELogLevel::Error, L"ChatServer", 0, std::source_location::current(),
				L"event=metrics_start_failed port=%u | 메트릭 리스너를 시작하지 못했습니다. 채팅 서버는 계속 시작합니다.",
				static_cast<unsigned>(metricsConfig.port));
		}
	}
	const auto stopMetrics = [&metricsExporter, &systemMetrics]
	{
		g_pLogger->SetPeriodicCallback({});
		metricsExporter.Stop();
		systemMetrics.reset();
	};

	MHLib::security::CEncryption::sm_packetKey = Chatting::kPacketKey;
	MHLib::utils::g_pProfileManager = MHLib::utils::CProfileManager::GetInstance();
	g_pLogger->WriteLog(ELogLevel::Info, L"ChatServer", 0, std::source_location::current(),
		L"event=server_config acceptex_count=%d iocp_worker_count=%d iocp_concurrency=%d contents_worker_count=%zu log_queue_capacity=%zu"
		L" | 서버 시작 설정을 적용했습니다.", networkSettings.acceptExCount, networkSettings.iocpWorkerCount,
		networkSettings.iocpConcurrency, contentsConfig.workerCount, loggingConfig.queueCapacity);

	auto *pServer = new Chatting::CChatServer(contentsConfig.workerCount, contentsConfig.frameInterval);
	if (!pServer->StartContents())
	{
		g_pLogger->WriteLog(ELogLevel::Error, L"ChatServer", 0, std::source_location::current(),
			L"event=contents_start_failed | 채팅 콘텐츠 런타임을 시작하지 못했습니다.");
		stopMetrics();
		g_pLogger->Stop();
		delete pServer;
		return 1;
	}
	ServerNetworkLib::g_pServer = pServer;
	if (systemMetrics)
	{
		g_pLogger->SetPeriodicCallback(metricsExporter.CreatePublishCallback(
			[pServer, &systemMetrics, &serverMetrics, &chatMetrics, lastSample = std::chrono::steady_clock::time_point{}]
			(MetricsLib::SMetricsSnapshot &outMetrics) mutable
			{
				const auto now = std::chrono::steady_clock::now();
				const bool hasPreviousSample = lastSample != std::chrono::steady_clock::time_point{};
				if (hasPreviousSample && now - lastSample < std::chrono::seconds{1})
					return false;

				// 목표 주기 대신 같은 수집 콜백의 실제 표본 간격을 사용한다.
				const double elapsedSeconds = hasPreviousSample ? std::chrono::duration<double>(now - lastSample).count() : 0.0;
				lastSample = now;
				serverMetrics.Collect(*pServer, elapsedSeconds, outMetrics);
				chatMetrics.Collect(elapsedSeconds, outMetrics);
				systemMetrics->Collect(outMetrics);
				return true;
			}));
	}
	std::atomic<bool> isReturned = false;
	std::atomic<bool> isStarted = false;
	std::thread serverThread([&]()
	{
		g_pLogger->RegisterLogProducer();
		isStarted = pServer->Start(networkSettings) != FALSE;
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
			pServer->StopContents();
			g_pLogger->WriteLog(ELogLevel::Info, L"ChatServer", 0, std::source_location::current(),
				L"event=server_process_exit | 서버 프로세스를 종료합니다. NetworkLib 정상 종료 처리는 보류 중입니다.");
			stopMetrics();
			fflush(nullptr);
			ExitProcess(0);
		}
		Sleep(10);
	}
	serverThread.join();
	pServer->StopContents();
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
