#include "pch.h"
#include "CommonProtocol.h"
#include "Config.h"
#include "SServerNetworkSettings.h"
#include "SContentsRuntimeSettings.h"
#include "MHLib/utils/SLoggingSettings.h"
#include "SMetricsSettings.h"
#include "ServerConfig.h"

namespace Chatting
{
	bool LoadServerConfig(const WCHAR *pFileName, ServerNetworkLib::SServerNetworkSettings &outNetwork,
		ContentsRuntimeLib::SContentsRuntimeSettings &outContents, MHLib::utils::SLoggingSettings &outLogging,
		MetricsLib::SMetricsSettings &outMetrics)
	{
		MHLib::utils::CFileLoader loader;
		if (!LoadConfigFile(pFileName, loader))
			return false;

		ServerNetworkLib::SServerNetworkSettings network{};
		ContentsRuntimeLib::SContentsRuntimeSettings contents{};
		MHLib::utils::SLoggingSettings logging{};
		MetricsLib::SMetricsSettings metrics{};
		const wchar_t *pError = nullptr;
		if (!ServerNetworkLib::SServerNetworkSettings::Load(loader, network, pError)
			|| !ContentsRuntimeLib::SContentsRuntimeSettings::Load(loader, contents, pError)
			|| !MHLib::utils::SLoggingSettings::Load(loader, logging, pError)
			|| !MetricsLib::SMetricsSettings::Load(loader, metrics, pError))
			return ConfigError(pFileName, pError);

		// 루프백 제한과 패킷 코드는 채팅 테스트 앱의 정책이다.
		if (network.ip != "127.0.0.1")
			return ConfigError(pFileName, L"Server: IP must be 127.0.0.1 (local test only)");
		network.packetCode = kPacketCode;

		outNetwork = std::move(network);
		outContents = contents;
		outLogging = logging;
		outMetrics = std::move(metrics);
		return true;
	}
}