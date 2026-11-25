#pragma once

namespace ServerNetworkLib
{
	class CServer;
}

namespace MetricsLib
{
	struct SMetricsSnapshot;
}

namespace ServerMetrics
{
	class CServerMetrics
	{
	public:
		// 로그 스레드에서만 구간 카운터를 읽고 리셋한다.
		void Collect(ServerNetworkLib::CServer &server, double elapsedSeconds, MetricsLib::SMetricsSnapshot &outMetrics);

	private:
		UINT64 m_acceptCompletions = 0;
		UINT64 m_receivedPackets = 0;
		UINT64 m_sentPackets = 0;
		UINT64 m_sendBatchLimitHits = 0;
	};
}