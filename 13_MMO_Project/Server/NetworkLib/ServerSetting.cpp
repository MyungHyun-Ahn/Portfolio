#include "pch.h"
#include "ServerSetting.h"

namespace NetworkLib
{
namespace serverSetting
{
	BYTE packetKey = 0xa9;
	BYTE packetCode = 0x77;
	std::string openIp;
	USHORT openPort;
	INT iocpWorkerThread = 16;
	INT iocpActiveThread = 4;
	INT useZeroCopy = 0;
	INT maxSessionCount = 20000;
	INT acceptexCount = 100;

	INT contentsThreadCount = 4;
	INT maxContentsFps = 25;
	INT delayFrame = 10;
}
}
