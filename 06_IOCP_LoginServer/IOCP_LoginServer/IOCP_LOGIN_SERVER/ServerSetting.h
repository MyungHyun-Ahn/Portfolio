#pragma once

namespace SERVER_SETTING
{
	extern BYTE PACKET_KEY;
	extern std::string openIP;
	extern USHORT openPort;
	extern INT IOCP_WORKER_THREAD;
	extern INT IOCP_ACTIVE_THREAD;
	extern INT USE_ZERO_COPY;
	extern INT MAX_SESSION_COUNT;
	extern INT ACCEPTEX_COUNT;


	extern BOOL monitorThreadRunning;
}