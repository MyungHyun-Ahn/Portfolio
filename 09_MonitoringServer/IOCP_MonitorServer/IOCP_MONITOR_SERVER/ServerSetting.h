#pragma once

namespace NET_SETTING
{
	extern BYTE PACKET_CODE;
	extern std::string openIP;
	extern USHORT openPort;
	extern INT IOCP_WORKER_THREAD;
	extern INT IOCP_ACTIVE_THREAD;
	extern INT USE_ZERO_COPY;
	extern INT MAX_SESSION_COUNT;
	extern INT ACCEPTEX_COUNT;
}

namespace LAN_SETTING
{
	extern std::string openIP;
	extern USHORT openPort;
	extern INT IOCP_WORKER_THREAD;
	extern INT IOCP_ACTIVE_THREAD;
	extern INT USE_ZERO_COPY;
	extern INT MAX_SESSION_COUNT;
	extern INT ACCEPTEX_COUNT;
};