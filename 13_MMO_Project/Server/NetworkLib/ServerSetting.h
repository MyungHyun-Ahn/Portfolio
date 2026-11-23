#pragma once

namespace ServerNetworkLib
{
namespace serverSetting
{
	extern BYTE packetCode;
	extern std::string openIp;
	extern USHORT openPort;
	extern INT iocpWorkerThread;
	extern INT iocpActiveThread;
	extern INT useZeroCopy;
	extern INT maxSessionCount;
	extern INT acceptexCount;

	// Contents
	extern INT contentsThreadCount;
	extern INT maxContentsFps;
	extern INT delayFrame;
}
}
