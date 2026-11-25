#pragma once

namespace NetworkCommonLib
{
	// LAN과 Net에서 함께 집계하는 네트워크 원시 카운터다.
	struct SNetworkCounters
	{
		LONG 	m_recvTps = 0;
		LONG 	m_sendTps = 0;
		LONG 	m_maxSendCount = 0;
		LONG 	m_acceptTps = 0;
		INT64 	m_acceptTotal = 0;
		INT64 	m_sendCallCount = 0;
		INT64 	m_sendPendingCount = 0;
	};

	inline SNetworkCounters g_networkCounters;
}
