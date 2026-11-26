#include "ServerNetworkLibPch.h"
#include "SServerNetworkSettings.h"

namespace ServerNetworkLib
{
	bool SServerNetworkSettings::Load(MHLib::utils::CFileLoader &loader, SServerNetworkSettings &outSettings,
		const wchar_t *&outError)
	{
		constexpr const wchar_t *kSection = L"Server";
		std::wstring ip;
		IN_ADDR address{};
		outError = L"Server: IP must be an IPv4 address";
		if (!loader.Load(kSection, L"IP", &ip) || InetPtonW(AF_INET, ip.c_str(), &address) != 1)
			return false;

		INT64 port = 0;
		outError = L"Server: PORT must be an integer (1..65535)";
		if (!loader.Load(kSection, L"PORT", &port) || port < 1 || port > 65535)
			return false;

		std::wstring mode;
		outError = L"Server: PACKET_MODE must be Net or Lan";
		if (!loader.Load(kSection, L"PACKET_MODE", &mode) || (mode != L"Net" && mode != L"Lan"))
			return false;

		INT64 maxSessionCount = 0;
		outError = L"Server: MAX_SESSION_COUNT must be an integer (1..65535)";
		if (!loader.Load(kSection, L"MAX_SESSION_COUNT", &maxSessionCount) || maxSessionCount < 1 || maxSessionCount > 65535)
			return false;

		INT64 acceptExCount = 0;
		outError = L"Server: ACCEPTEX_COUNT must be an integer (1..65535)";
		if (!loader.Load(kSection, L"ACCEPTEX_COUNT", &acceptExCount) || acceptExCount < 1 || acceptExCount > 65535)
			return false;

		INT64 iocpWorkerCount = 0;
		outError = L"Server: IOCP_WORKER_COUNT must be an integer (1..64)";
		if (!loader.Load(kSection, L"IOCP_WORKER_COUNT", &iocpWorkerCount) || iocpWorkerCount < 1 || iocpWorkerCount > MAXIMUM_WAIT_OBJECTS)
			return false;

		INT64 iocpConcurrency = 0;
		outError = L"Server: IOCP_CONCURRENCY must be an integer (1..IOCP_WORKER_COUNT)";
		if (!loader.Load(kSection, L"IOCP_CONCURRENCY", &iocpConcurrency) || iocpConcurrency < 1 || iocpConcurrency > iocpWorkerCount)
			return false;

		outSettings.ip.assign(ip.begin(), ip.end());
		outSettings.port = static_cast<USHORT>(port);
		outSettings.packetMode = mode == L"Lan" ? NetworkCommonLib::EPacketMode::Lan : NetworkCommonLib::EPacketMode::Net;
		outSettings.maxSessionCount = static_cast<INT>(maxSessionCount);
		outSettings.acceptExCount = static_cast<INT>(acceptExCount);
		outSettings.iocpWorkerCount = static_cast<INT>(iocpWorkerCount);
		outSettings.iocpConcurrency = static_cast<INT>(iocpConcurrency);
		outError = nullptr;
		return true;
	}
}
