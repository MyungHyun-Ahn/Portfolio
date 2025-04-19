#pragma once


namespace SERVER_SETTING
{
	extern BYTE PACKET_CODE;
	extern std::string openIP;
	extern USHORT openPort;
	extern INT IOCP_WORKER_THREAD;
	extern INT IOCP_ACTIVE_THREAD;
	extern INT USE_ZERO_COPY;
	extern INT USE_NAGLE;
	extern INT MAX_SESSION_COUNT;
	extern INT ACCEPTEX_COUNT;
}