#include "pch.h"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <new>
#include <random>
#include <shared_mutex>
#include <stdexcept>
#include <utility>
#include "CSerializableBuffer.h"
#include "NetworkLimits.h"
#include "CommonProtocol.h"
#include "CClientManager.h"
#include "CClient.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "ClientPacketDispatch.g.h"
#include "CChatNetClient.g.h"
#include "Config.h"
#include "SDummyConfig.h"
#include "SDummyRunStats.h"
#include "CChatDummyClient.h"
#include "CDummyRunner.h"

namespace Chatting
{
	namespace
	{
		using TClock = std::chrono::steady_clock;
		using TMilliseconds = std::chrono::milliseconds;
		using MHLib::utils::ELogLevel;
		using MHLib::utils::g_pLogger;
		constexpr auto kSection = L"DummyClient";
		constexpr DWORD kReconnectScale = 10000;

		// 각 슬롯의 실행 상태는 한 앱 워커만 읽고 갱신한다.
		struct SDummySlot
		{
			std::unique_ptr<CChatDummyClient> m_client;
			TClock::time_point m_nextAction{};			// 다음 행동을 보낼 시간
			TClock::time_point m_nextHeartbeat{};		// 하트비트를 보낼 시간
			bool m_isWaitingToReconnect = false;		// 재접속을 결정하고, 기존 연결이 끝나길 기다리는 상태
			bool m_isReconnecting = false;
			bool m_wasInitiallyReady = false;
		};


		template<class TValue>
		bool LoadNumber(MHLib::utils::CFileLoader &loader, const WCHAR *pKey, INT64 minValue, INT64 maxValue,
			TValue &outValue, bool isRequired = false)
		{
			std::wstring text;
			if (!loader.Load(kSection, pKey, &text))
				return !isRequired;
			INT64 number = 0;
			if (!loader.Load(kSection, pKey, &number) || number < minValue || number > maxValue)
				return false;
			outValue = static_cast<TValue>(number);
			return true;
		}
	}

	const char *CDummyRunner::loadConfig(const std::filesystem::path &path)
	{
		if (!CheckConfigFile(path.c_str()))
			return "config_file_unreadable_or_invalid_encoding";
		MHLib::utils::CFileLoader loader;
		if (!loader.Parse(path.c_str()))
			return "config_parse_failed";
		std::wstring text;
		IN_ADDR address{};
		if (!loader.Load(kSection, L"IP", &text) || InetPtonW(AF_INET, text.c_str(), &address) != 1
			|| address.S_un.S_un_b.s_b1 != 127)
			return "config_loopback_required";
		SDummyConfig config;
		config.m_ip.assign(text.begin(), text.end());
		if (loader.Load(kSection, L"SCENARIO", &text))
			return "config_scenario_removed";
		if (loader.Load(kSection, L"PACKET_MODE", &text))
		{
			if (text == L"Lan")
				config.m_packetMode = NetworkCommonLib::EPacketMode::Lan;
			else if (text != L"Net")
				return "config_packet_mode_invalid";
		}

		constexpr INT64 kMaxAccountNo = (std::numeric_limits<INT64>::max)();
		INT64 clientCount = static_cast<INT64>(config.m_clientCount);
		if (!LoadNumber(loader, L"CLIENT_COUNT", 1, kMaxAccountNo, clientCount)
			|| static_cast<UINT64>(clientCount) > (std::numeric_limits<size_t>::max)()
			|| !LoadNumber(loader, L"PORT", 1, 65535, config.m_port, true))
			return "config_port_or_client_count_invalid";
		config.m_clientCount = static_cast<size_t>(clientCount);
		if (!LoadNumber(loader, L"ACCOUNT_BASE", 1, kMaxAccountNo - (clientCount - 1), config.m_accountBase, true))
			return "config_account_base_invalid";
		if (!LoadNumber(loader, L"SECTOR_X", 0, kMaxSectorX - 1, config.m_sectorX)
			|| !LoadNumber(loader, L"SECTOR_Y", 0, kMaxSectorY - 1, config.m_sectorY))
			return "config_sector_invalid";
		constexpr int kMinRecvBufferBytes = static_cast<int>(NetworkCommonLib::CSerializableBuffer::EBufferDefine::PacketMaxSize);
		if (!LoadNumber(loader, L"WORKER_COUNT", 1, 16, config.m_workerCount)
			|| !LoadNumber(loader, L"APP_WORKER_COUNT", 1, 16, config.m_appWorkerCount)
			|| !LoadNumber(loader, L"RECV_BUFFER_BYTES", kMinRecvBufferBytes, NetworkCommonLib::kMaxRecvBufferBytes, config.m_recvBufferBytes))
			return "config_network_limits_invalid";
		if (!LoadNumber(loader, L"CONNECT_INTERVAL_MS", 1, MAXDWORD, config.m_connectIntervalMilliseconds)
			|| !LoadNumber(loader, L"CONNECT_ATTEMPTS_PER_INTERVAL", 1, MAXDWORD, config.m_connectAttemptsPerInterval))
			return "config_connect_schedule_invalid";
		if (!LoadNumber(loader, L"CONNECT_TIMEOUT_MS", 1, MAXDWORD, config.m_connectTimeoutMilliseconds)
			|| !LoadNumber(loader, L"LOGIN_TIMEOUT_MS", 1, MAXDWORD, config.m_loginTimeoutMilliseconds)
			|| !LoadNumber(loader, L"SECTOR_TIMEOUT_MS", 1, MAXDWORD, config.m_sectorTimeoutMilliseconds)
			|| !LoadNumber(loader, L"DELIVERY_TIMEOUT_MS", 1, MAXDWORD, config.m_deliveryTimeoutMilliseconds)
			|| !LoadNumber(loader, L"DRAIN_TIMEOUT_MS", 1, MAXDWORD, config.m_drainTimeoutMilliseconds)
			|| !LoadNumber(loader, L"TICK_INTERVAL_MS", 1, 100, config.m_tickIntervalMilliseconds))
			return "config_timeout_or_tick_invalid";
		if (!LoadNumber(loader, L"RUN_DURATION_MS", 1, MAXDWORD, config.m_runDurationMilliseconds)
			|| !LoadNumber(loader, L"ACTION_INTERVAL_MS", 1, MAXDWORD, config.m_actionIntervalMilliseconds)
			|| !LoadNumber(loader, L"HEARTBEAT_INTERVAL_MS", 1, kReceiveTimeoutMilliseconds - 1, config.m_heartbeatIntervalMilliseconds))
			return "config_schedule_invalid";
		if (!LoadNumber(loader, L"SECTOR_MOVE_WEIGHT", 0, MAXDWORD, config.m_sectorMoveWeight)
			|| !LoadNumber(loader, L"MESSAGE_WEIGHT", 0, MAXDWORD, config.m_messageWeight)
			|| (config.m_sectorMoveWeight == 0 && config.m_messageWeight == 0))
			return "config_action_weights_invalid";
		if (!LoadNumber(loader, L"RECONNECT_PROBABILITY_BPS", 0, kReconnectScale, config.m_reconnectProbabilityBps))
			return "config_reconnect_probability_invalid";
		m_config = std::move(config);
		return nullptr;
	}

	void CDummyRunner::runClients(const std::wstring &runMarker, TClock::time_point deadline)
	{
		struct SAppWork
		{
			SDummyRunStats m_stats;
			std::vector<SDummySlot> m_slots;
		};
		ClientNetworkLib::CClientManager network;
		const size_t appWorkerCount = (std::min)(m_config.m_clientCount, static_cast<size_t>(m_config.m_appWorkerCount));
		std::vector<SAppWork> work;
		std::vector<std::thread> workers;
		try
		{
			work.resize(appWorkerCount);
			workers.reserve(appWorkerCount);
			m_appWorkers.resize(appWorkerCount);
			for (size_t workerIndex = 0; workerIndex < work.size(); ++workerIndex)
			{
				auto &batch = work[workerIndex];
				const size_t slotCount = m_config.m_clientCount / work.size() + (workerIndex < m_config.m_clientCount % work.size());
				batch.m_slots.reserve(slotCount);
				for (size_t index = workerIndex; index < m_config.m_clientCount; index += work.size())
					batch.m_slots.push_back({std::make_unique<CChatDummyClient>(network, m_config, batch.m_stats, index, runMarker)});
			}
		}
		catch (const std::length_error &)
		{
			m_appWorkers.clear();
			m_stats.Fail("client_storage_size_unavailable");
			return;
		}
		catch (const std::bad_alloc &)
		{
			m_appWorkers.clear();
			m_stats.Fail("client_storage_allocation_failed");
			return;
		}
		MHLib::security::CEncryption::sm_packetKey = kPacketKey;
		if (!network.Init(m_config.m_workerCount, m_config.m_recvBufferBytes, m_config.m_packetMode))
		{
			m_appWorkers.clear();
			m_stats.Fail("network_init_failed");
			return;
		}

		enum class ELaunch { Preparing, Start, Abort };
		std::atomic<ELaunch> launch{ELaunch::Preparing};
		std::atomic<bool> hasFailed{false};
		std::shared_mutex admissionRWLock;
		bool isAdmissionClosed = false;
		std::mutex connectScheduleLock;
		TClock::time_point nextConnectInterval{};
		DWORD remainingConnectAttempts = 0;
		const TMilliseconds connectInterval(m_config.m_connectIntervalMilliseconds);
		const TMilliseconds actionInterval(m_config.m_actionIntervalMilliseconds);
		const TMilliseconds heartbeatInterval(m_config.m_heartbeatIntervalMilliseconds);
		const TMilliseconds drainTimeout(m_config.m_drainTimeoutMilliseconds);
		const auto pause = [&] { std::this_thread::sleep_for(TMilliseconds(m_config.m_tickIntervalMilliseconds)); };

		// 접속 시도 함수
		const auto tryStartConnection = [&](CChatDummyClient &client)
		{
			// 실제 접속 제출까지 한도를 공유한다. 응답 대기나 Tick은 락 밖에서 처리한다.
			std::lock_guard scheduleLock(connectScheduleLock);
			const auto now = TClock::now();
			if (hasFailed || now >= deadline)
				return false;
			if (now >= nextConnectInterval)
			{
				nextConnectInterval = now + connectInterval;
				remainingConnectAttempts = m_config.m_connectAttemptsPerInterval;
			}
			if (remainingConnectAttempts == 0)
				return false;
			--remainingConnectAttempts;
			if (client.Start())
				return true;
			hasFailed = true;
			return false;
		};

		const auto runWorker = [&](size_t workerIndex) noexcept
		{
			launch.wait(ELaunch::Preparing);
			if (launch.load() == ELaunch::Abort)
				return;

			auto &[stats, slots] = work[workerIndex];
			auto &result = m_appWorkers[workerIndex];
			result.m_threadId = GetCurrentThreadId();
			result.m_clientCount = slots.size();
			const auto seedTime = static_cast<UINT64>(TClock::now().time_since_epoch().count());
			std::mt19937 random(static_cast<DWORD>(seedTime ^ (seedTime >> 32) ^ result.m_threadId));
			const UINT64 totalWeight = static_cast<UINT64>(m_config.m_sectorMoveWeight) + m_config.m_messageWeight;
			std::uniform_int_distribution<UINT64> pickAction(0, totalWeight - 1);
			std::uniform_int_distribution<DWORD> pickReconnect(0, kReconnectScale - 1);
			std::uniform_int_distribution<int> pickSector(0, kMaxSectorX * kMaxSectorY - 2);

			const auto pump = [&]
			{
				for (auto &slot : slots)
				{
					auto &client = *slot.m_client;
					const bool wasReady = client.IsReady();
					client.Pump();
					if (wasReady || !client.IsReady())
						continue;
					const auto now = TClock::now();
					slot.m_nextAction = slot.m_wasInitiallyReady ? now + actionInterval : now;
					slot.m_nextHeartbeat = now + heartbeatInterval;
					if (!slot.m_wasInitiallyReady)
					{
						slot.m_wasInitiallyReady = true;
						++result.m_firstReady;

					}
					else if (slot.m_isReconnecting)
					{
						++stats.m_reconnectReady;
						slot.m_isReconnecting = false;
					}
				}
				if (stats.m_failureCount != 0)
					hasFailed = true;
			};
			const auto heartbeat = [&](SDummySlot &slot, TClock::time_point now)
			{
				if (slot.m_client->IsReady() && now >= slot.m_nextHeartbeat)
				{
					slot.m_client->SendHeartbeat();
					slot.m_nextHeartbeat = now + heartbeatInterval;
				}
			};
			size_t nextInitialConnect = 0;
			while (!hasFailed && TClock::now() < deadline)
			{
				pump();
				while (nextInitialConnect < slots.size())
				{
					std::shared_lock admissionLock(admissionRWLock);
					if (isAdmissionClosed || !tryStartConnection(*slots[nextInitialConnect].m_client))
						break;
					++nextInitialConnect;
				}
				for (auto &slot : slots)
				{
					// 제출 호출까지 공유 락 안에서 끝낸다. Tick과 drain은 이 락을 잡지 않는다.
					std::shared_lock admissionLock(admissionRWLock);
					const auto now = TClock::now();
					if (isAdmissionClosed || now >= deadline || hasFailed)
						break;
					auto &client = *slot.m_client;
					if (slot.m_isWaitingToReconnect && !client.IsActive())
					{
						if (tryStartConnection(client))
						{
							slot.m_isWaitingToReconnect = false;
							slot.m_isReconnecting = true;
							++stats.m_reconnectStarted;
						}
						continue;
					}
					if (!client.IsReady())
						continue;
					if (client.TakeActionCompletion() && pickReconnect(random) < m_config.m_reconnectProbabilityBps)
					{
						++stats.m_reconnectRequested;
						slot.m_isWaitingToReconnect = true;
						client.Close();
						continue;
					}
					heartbeat(slot, now);
					if (stats.m_failureCount == 0 && !client.HasPendingAction() && now >= slot.m_nextAction
						&& TClock::now() < deadline && !hasFailed)
					{
						slot.m_nextAction = now + actionInterval;
						if (pickAction(random) < m_config.m_sectorMoveWeight)
						{
							int sector = pickSector(random);
							if (sector >= client.GetSectorIndex())
								++sector;
							client.StartMove(sector);
						}
						else
							client.StartMessage();
					}
					if (stats.m_failureCount != 0)
						hasFailed = true;
				}
				if (!hasFailed)
					pause();
			}

			// 대기 중인 재접속은 시작하지 않되, 이미 제출한 재접속은 Ready까지 처리한다.
			const auto hasPendingWork = [&]
			{
				return std::any_of(slots.begin(), slots.end(), [](const SDummySlot &slot)
				{
					return slot.m_client->HasPendingAction() || slot.m_isReconnecting;
				});
			};
			const auto workDeadline = deadline + drainTimeout;
			while (!hasFailed && hasPendingWork() && TClock::now() < workDeadline)
			{
				pump();
				if (!hasFailed && hasPendingWork())
					pause();
			}
			if (!hasFailed && hasPendingWork())
			{
				stats.Fail("work_drain_timeout");
				hasFailed = true;
			}

			for (auto &slot : slots)
				slot.m_client->Close();
			const auto hasActiveClients = [&]
			{
				return std::any_of(slots.begin(), slots.end(), [](const SDummySlot &slot) { return slot.m_client->IsActive(); });
			};
			const auto terminalDeadline = TClock::now() + drainTimeout;
			while (hasActiveClients() && TClock::now() < terminalDeadline)
			{
				pump();
				if (hasActiveClients())
					pause();
			}
			if (hasActiveClients())
				stats.Fail("terminal_timeout");
			result.m_terminals = stats.m_stages[static_cast<size_t>(EChatPhase::Terminal)];
			result.m_actionsCompleted = stats.m_moveCompleted + stats.m_messageCompleted;
		};

		try
		{
			for (size_t workerIndex = 0; workerIndex < work.size(); ++workerIndex)
				workers.emplace_back(runWorker, workerIndex);
			launch.store(ELaunch::Start);
		}
		catch (const std::exception &)
		{
			m_stats.Fail("app_worker_start_failed");
			m_appWorkers.clear();
			launch.store(ELaunch::Abort);
		}
		launch.notify_all();

		if (launch.load() == ELaunch::Start)
		{
			while (!hasFailed && TClock::now() < deadline)
				pause();
			{
				// cutoff 전에 진입한 제출까지 반환한 뒤 새 작업이 없음을 알린다.
				std::unique_lock admissionLock(admissionRWLock);
				isAdmissionClosed = true;
			}
			if (!hasFailed)
				g_pLogger->WriteLogConsole(ELogLevel::Info, L"event=dummy_draining | 새 작업을 중단하고 진행 중인 작업을 정리합니다.");
		}
		for (auto &worker : workers)
			worker.join();
		for (const auto &batch : work)
			m_stats.Merge(batch.m_stats);
		network.Stop();
	}

	bool CDummyRunner::saveSummary(const std::filesystem::path &path, const std::string &runId,
		long long startUnixMilliseconds, int exitCode) const
	{
		const auto endUnixMilliseconds = std::chrono::duration_cast<TMilliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		std::ofstream file(path, std::ios::binary);
		file << "{\n  \"run_id\": \"" << runId << "\", \"exit_code\": " << exitCode
			<< ", \"start_unix_ms\": " << startUnixMilliseconds << ", \"end_unix_ms\": " << endUnixMilliseconds
			<< ",\n  \"settings\": {\"ip\": \"" << m_config.m_ip << "\", \"port\": " << m_config.m_port
			<< ", \"client_count\": " << m_config.m_clientCount << ", \"account_base\": " << m_config.m_accountBase
			<< ", \"sector\": [" << m_config.m_sectorX << ", " << m_config.m_sectorY << "]"
			<< ", \"workers\": " << m_config.m_workerCount << ", \"recv_buffer_bytes\": " << m_config.m_recvBufferBytes
			<< ", \"app_worker_count\": " << m_config.m_appWorkerCount
			<< ", \"connect_interval_ms\": " << m_config.m_connectIntervalMilliseconds
			<< ", \"connect_attempts_per_interval\": " << m_config.m_connectAttemptsPerInterval
			<< ", \"connect_timeout_ms\": " << m_config.m_connectTimeoutMilliseconds
			<< ", \"login_timeout_ms\": " << m_config.m_loginTimeoutMilliseconds
			<< ", \"sector_timeout_ms\": " << m_config.m_sectorTimeoutMilliseconds
			<< ", \"delivery_timeout_ms\": " << m_config.m_deliveryTimeoutMilliseconds
			<< ", \"drain_timeout_ms\": " << m_config.m_drainTimeoutMilliseconds
			<< ", \"tick_interval_ms\": " << m_config.m_tickIntervalMilliseconds
			<< ", \"run_duration_ms\": " << m_config.m_runDurationMilliseconds
			<< ", \"action_interval_ms\": " << m_config.m_actionIntervalMilliseconds
			<< ", \"heartbeat_interval_ms\": " << m_config.m_heartbeatIntervalMilliseconds
			<< ", \"sector_move_weight\": " << m_config.m_sectorMoveWeight << ", \"message_weight\": " << m_config.m_messageWeight
			<< ", \"reconnect_probability_bps\": " << m_config.m_reconnectProbabilityBps << "},\n  \"stages\": {";
		for (size_t index = 1; index < kPhaseNames.size(); ++index)
			file << (index == 1 ? "" : ", ") << '"' << kPhaseNames[index] << "\": " << m_stats.m_stages[index];
		file << "},\n  \"actions\": {\"move_submitted\": " << m_stats.m_moveSubmitted << ", \"move_completed\": " << m_stats.m_moveCompleted
			<< ", \"message_submitted\": " << m_stats.m_messageSubmitted << ", \"message_completed\": " << m_stats.m_messageCompleted
			<< ", \"peer_received\": " << m_stats.m_peerReceived << ", \"heartbeat_submitted\": " << m_stats.m_heartbeatSubmitted
			<< "},\n  \"message_rtt_ms\": {\"count\": " << m_stats.m_messageCompleted;
		if (m_stats.m_messageCompleted == 0)
		{
			file << ", \"min\": null, \"mean\": null, \"p50\": null, \"p95\": null, \"p99\": null, \"max\": null";
		}
		else
		{
			constexpr double kMicrosecondsPerMillisecond = 1000.0;
			file << ", \"min\": " << m_stats.m_messageRttMinMicroseconds / kMicrosecondsPerMillisecond
				<< ", \"mean\": " << m_stats.m_messageRttSumMicroseconds / kMicrosecondsPerMillisecond / m_stats.m_messageCompleted
				<< ", \"p50\": " << m_stats.GetMessageRttPercentileMicroseconds(50) / kMicrosecondsPerMillisecond
				<< ", \"p95\": " << m_stats.GetMessageRttPercentileMicroseconds(95) / kMicrosecondsPerMillisecond
				<< ", \"p99\": " << m_stats.GetMessageRttPercentileMicroseconds(99) / kMicrosecondsPerMillisecond
				<< ", \"max\": " << m_stats.m_messageRttMaxMicroseconds / kMicrosecondsPerMillisecond;
		}
		file << "},\n  \"reconnects\": {\"requested\": " << m_stats.m_reconnectRequested << ", \"started\": " << m_stats.m_reconnectStarted
			<< ", \"ready\": " << m_stats.m_reconnectReady << "},\n  \"failure_count\": " << m_stats.m_failureCount
			<< ",\n  \"failures\": [";
		for (size_t index = 0; index < m_stats.m_failureCount && index < m_stats.m_failures.size(); ++index)
		{
			const auto &failure = m_stats.m_failures[index];
			file << (index == 0 ? "" : ", ") << "{\"reason\": \"" << failure.m_pReason
				<< "\", \"account\": " << failure.m_accountNo
				<< ", \"phase\": \"" << kPhaseNames[static_cast<size_t>(failure.m_phase)] << "\", \"error\": " << failure.m_error << '}';
		}
		file << "],\n  \"app_workers\": [";
		for (size_t index = 0; index < m_appWorkers.size(); ++index)
		{
			const auto &worker = m_appWorkers[index];
			file << (index == 0 ? "" : ", ") << "{\"thread_id\": " << worker.m_threadId
				<< ", \"clients\": " << worker.m_clientCount << ", \"first_ready\": " << worker.m_firstReady
				<< ", \"terminals\": " << worker.m_terminals << ", \"actions_completed\": " << worker.m_actionsCompleted << '}';
		}
		file << "]\n}\n";
		file.close();
		return static_cast<bool>(file);
	}

	int CDummyRunner::Run(TClock::time_point processStart)
	{
		m_appWorkers.clear();
		m_stats = {};
		m_config = {};
		const auto start = std::chrono::duration_cast<TMilliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		const std::string runId = std::to_string(start) + "-" + std::to_string(GetCurrentProcessId());
		std::wstring executableDirectory;
		if (!GetExecutableDirectory(executableDirectory))
		{
			g_pLogger->WriteLogConsole(ELogLevel::Error, L"event=module_path_failed | 실행 파일 경로를 확인하지 못했습니다.");
			return 1;
		}
		const std::filesystem::path directory(executableDirectory);
		const auto runDirectory = directory / L"DummyReports" / runId;
		std::error_code error;
		if (!std::filesystem::create_directories(runDirectory, error))
		{
			g_pLogger->WriteLogConsole(ELogLevel::Error, L"event=report_directory_failed | 결과 디렉터리를 만들지 못했습니다: %ls", runDirectory.c_str());
			return 1;
		}
		const auto report = runDirectory / L"RunSummary.json";
		g_pLogger->WriteLogConsole(ELogLevel::Info, L"event=dummy_start | 결과 파일: %ls", report.c_str());
		int exitCode = 0;
		if (const char *pError = loadConfig(directory / L"DummyClientConfig.conf"))
		{
			m_stats.Fail(pError);
			exitCode = 2;
		}
		else
		{
			const auto deadline = processStart + TMilliseconds(m_config.m_runDurationMilliseconds);
			runClients(L"dummy:" + std::wstring(runId.begin(), runId.end()) + L":", deadline);
			exitCode = m_stats.m_failureCount == 0 ? 0 : 1;
		}
		if (!saveSummary(report, runId, start, exitCode))
		{
			g_pLogger->WriteLogConsole(ELogLevel::Error, L"event=report_write_failed | 결과 파일을 저장하지 못했습니다: %ls", report.c_str());
			return 1;
		}
		g_pLogger->WriteLogConsole(exitCode == 0 ? ELogLevel::Info : ELogLevel::Error,
			L"event=dummy_finished exit_code=%d | 더미 실행을 종료했습니다.", exitCode);
		return exitCode;
	}
}
