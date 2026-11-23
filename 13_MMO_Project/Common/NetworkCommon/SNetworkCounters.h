#pragma once

namespace NetworkCommonLib
{
	// LAN과 Net 전송에서 함께 집계하는 카운터.
	struct SNetworkCounters
	{
		LONG m_recvTps = 0;
		LONG m_sendTps = 0;
		LONG m_maxSendCount = 0;
	};

	inline SNetworkCounters g_networkCounters;
}
