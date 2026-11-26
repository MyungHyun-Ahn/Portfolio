#include "pch.h"
#include <filesystem>
#include <limits>
#include <utility>
#include "CommonProtocol.h"
#include "Config.h"
#include "SClientNetworkSettings.h"
#include "SDummyConfig.h"

namespace Chatting
{
	namespace
	{
		constexpr auto kSection = L"DummyClient";

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

	const char *SDummyConfig::Load(const std::filesystem::path &path, ClientNetworkLib::SClientNetworkSettings &outNetwork,
		SDummyConfig &outConfig)
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
		ClientNetworkLib::SClientNetworkSettings network{};
		const wchar_t *pError = nullptr;
		if (!ClientNetworkLib::SClientNetworkSettings::Load(loader, network, pError))
		{
			ConfigError(L"DummyClientConfig.conf", pError);
			return pError == ClientNetworkLib::SClientNetworkSettings::kPacketModeError
				? "config_packet_mode_invalid" : "config_network_limits_invalid";
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
		if (!LoadNumber(loader, L"APP_WORKER_COUNT", 1, 16, config.m_appWorkerCount))
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
		outNetwork = network;
		outConfig = std::move(config);
		return nullptr;
	}
}
