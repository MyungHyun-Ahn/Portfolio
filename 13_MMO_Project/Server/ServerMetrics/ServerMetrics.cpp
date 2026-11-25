#include "ServerNetworkLibPch.h"
#include "MetricsSnapshot.h"
#include "SNetworkCounters.h"
#include "ServerMetrics.h"

namespace ServerMetrics
{
	namespace
	{
		// 교환한 증가분은 첫 표본에도 누계에 더하고 처리율은 유효한 구간에서만 추가한다.
		void AppendIntervalMetric(const std::string &name, LONG &rawCount, UINT64 &total, double elapsedSeconds,
			MetricsLib::SMetricsSnapshot &outMetrics)
		{
			const auto count = static_cast<ULONG>(InterlockedExchange(&rawCount, 0));
			total += count;
			outMetrics.m_counters.push_back({name + "_total", total, {}});
			if (elapsedSeconds > 0.0)
				outMetrics.m_gauges.push_back({name + "_per_second", static_cast<double>(count) / elapsedSeconds, {}});
		}
	}

	void CServerMetrics::Collect(ServerNetworkLib::CServer &server, double elapsedSeconds, MetricsLib::SMetricsSnapshot &outMetrics)
	{
		auto &networkCounters = NetworkCommonLib::g_networkCounters;
		AppendIntervalMetric("net_accept_completions", networkCounters.m_acceptTps, m_acceptCompletions, elapsedSeconds, outMetrics);
		AppendIntervalMetric("net_received_packets", networkCounters.m_recvTps, m_receivedPackets, elapsedSeconds, outMetrics);
		AppendIntervalMetric("net_sent_packets", networkCounters.m_sendTps, m_sentPackets, elapsedSeconds, outMetrics);
		AppendIntervalMetric("net_send_batch_limit_hits", networkCounters.m_maxSendCount, m_sendBatchLimitHits, elapsedSeconds, outMetrics);

		const auto accepted = static_cast<UINT64>(InterlockedCompareExchange64(&networkCounters.m_acceptTotal, 0, 0));
		const auto sendCalls = static_cast<UINT64>(InterlockedCompareExchange64(&networkCounters.m_sendCallCount, 0, 0));
		const auto sendPending = static_cast<UINT64>(InterlockedCompareExchange64(&networkCounters.m_sendPendingCount, 0, 0));
		auto &counters = outMetrics.m_counters;
		counters.push_back({"net_sessions_accepted_total", accepted, {}});
		counters.push_back({"net_wsasend_calls_total", sendCalls, {}});
		counters.push_back({"net_wsasend_pending_total", sendPending, {}});
		outMetrics.m_gauges.push_back({"net_sessions", static_cast<double>(server.GetSessionCount()), {}});
	}
}