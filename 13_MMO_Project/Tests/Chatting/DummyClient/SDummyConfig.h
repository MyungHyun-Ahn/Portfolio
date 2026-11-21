#pragma once

namespace Chatting
{
	struct SDummyConfig
	{
		std::string m_ip;
		USHORT m_port = 0;
		size_t m_clientCount = 2;
		INT64 m_accountBase = 0;
		WORD m_sectorX = 10;
		WORD m_sectorY = 10;
		int m_workerCount = 1;
		int m_appWorkerCount = 1;
		int m_recvBufferBytes = 4096;
		DWORD m_connectIntervalMilliseconds = 100;
		DWORD m_connectAttemptsPerInterval = 50;
		DWORD m_connectTimeoutMilliseconds = 5000;
		DWORD m_loginTimeoutMilliseconds = 5000;
		DWORD m_sectorTimeoutMilliseconds = 5000;
		DWORD m_deliveryTimeoutMilliseconds = 3000;
		DWORD m_drainTimeoutMilliseconds = 5000;
		DWORD m_tickIntervalMilliseconds = 1;
		DWORD m_runDurationMilliseconds = 60000;
		DWORD m_actionIntervalMilliseconds = 1000;
		DWORD m_heartbeatIntervalMilliseconds = 30000;
		DWORD m_sectorMoveWeight = 1;
		DWORD m_messageWeight = 1;
		DWORD m_reconnectProbabilityBps = 0;
	};
}
