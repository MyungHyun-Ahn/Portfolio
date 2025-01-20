#include "pch.h"
#include "ServerSetting.h"

BYTE PACKET_KEY = 0x77;

std::string openIP;
USHORT openPort;

INT IOCP_WORKER_THREAD = 16;
INT IOCP_ACTIVE_THREAD = 4;

INT USE_ZERO_COPY = 0;

INT MAX_SESSION_COUNT = 20000;

INT ACCEPTEX_COUNT = 100;

BOOL monitorThreadRunning = TRUE;