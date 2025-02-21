#include "pch.h"
#include "ClientSetting.h"

namespace CLIENT_SETTING
{
	BYTE PACKET_KEY = 0x77;
	INT IOCP_WORKER_THREAD = 16;
	INT IOCP_ACTIVE_THREAD = 4;
	INT USE_ZERO_COPY = 0;
}