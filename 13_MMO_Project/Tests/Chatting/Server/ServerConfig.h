#pragma once

namespace ServerNetworkLib
{
	struct SServerNetworkSettings;
}

namespace ContentsRuntimeLib
{
	struct SContentsRuntimeSettings;
}

namespace MHLib::utils
{
	struct SLoggingSettings;
}

namespace MetricsLib
{
	struct SMetricsSettings;
}

namespace Chatting
{
	// 서버 설정을 한 번 읽고 모든 검증이 성공했을 때만 결과를 넘긴다.
	[[nodiscard]]
	bool LoadServerConfig(const WCHAR *pFileName, ServerNetworkLib::SServerNetworkSettings &outNetwork,
		ContentsRuntimeLib::SContentsRuntimeSettings &outContents, MHLib::utils::SLoggingSettings &outLogging,
		MetricsLib::SMetricsSettings &outMetrics);
}