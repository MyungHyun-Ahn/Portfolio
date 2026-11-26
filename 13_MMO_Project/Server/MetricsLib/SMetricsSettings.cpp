#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <share.h>
#include <string>
#include <unordered_map>

// 기존 로더의 C4244 억제를 이 파일의 설정 코드로 전파하지 않는다.
#pragma warning(push)
#include "MHLib/utils/CFileLoader.h"
#pragma warning(pop)
#include "SMetricsSettings.h"

#pragma comment(lib, "Ws2_32.lib")

namespace MetricsLib
{
	bool SMetricsSettings::Load(MHLib::utils::CFileLoader &loader, SMetricsSettings &outSettings, const wchar_t *&outError)
	{
		constexpr const wchar_t *kSection = L"Server";
		INT64 port = 0;
		outError = L"Server: METRICS_PORT must be an integer (0..65535; 0 disables metrics)";
		if (!loader.Load(kSection, L"METRICS_PORT", &port) || port < 0 || port > 65535)
			return false;

		std::wstring bindAddress;
		IN_ADDR address{};
		outError = L"Server: METRICS_BIND_ADDRESS must be a specific IPv4 address";
		if (!loader.Load(kSection, L"METRICS_BIND_ADDRESS", &bindAddress)
			|| InetPtonW(AF_INET, bindAddress.c_str(), &address) != 1 || address.S_un.S_addr == INADDR_ANY)
			return false;

		outSettings.bindAddress.assign(bindAddress.begin(), bindAddress.end());
		outSettings.port = static_cast<std::uint16_t>(port);
		outError = nullptr;
		return true;
	}
}
