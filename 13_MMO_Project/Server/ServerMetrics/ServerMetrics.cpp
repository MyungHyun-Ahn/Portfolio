#include "ServerNetworkLibPch.h"
#include "MetricsSnapshot.h"
#include "ServerMetrics.h"

namespace ServerMetrics
{
	void AppendIntervalMetric(const ServerNetworkLib::SMonitorSnapshot &sample, ServerNetworkLib::EMonitorInterval interval,
		const std::string &name, MetricsLib::SMetricsSnapshot &outMetrics)
	{
		const auto index = static_cast<size_t>(interval);
		if (index >= ServerNetworkLib::SMonitorSnapshot::kIntervalCount)
			return;
		outMetrics.m_counters.push_back({name + "_total", sample.m_totals[index], {}});
		if (sample.m_hasIntervalRates)
			outMetrics.m_gauges.push_back({name + "_per_second", sample.m_rates[index], {}});
	}

	void AppendCommonMetrics(const ServerNetworkLib::SMonitorSnapshot &sample, MetricsLib::SMetricsSnapshot &outMetrics)
	{
		using ServerNetworkLib::EMonitorInterval;
		AppendIntervalMetric(sample, EMonitorInterval::AcceptCompletions, "game_net_accept_completions", outMetrics);
		AppendIntervalMetric(sample, EMonitorInterval::ReceivedPackets, "game_net_received_packets", outMetrics);
		AppendIntervalMetric(sample, EMonitorInterval::SentPackets, "game_net_sent_packets", outMetrics);
		// 작업 분배는 공통 지표지만 기존 수집 이름은 유지한다.
		AppendIntervalMetric(sample, EMonitorInterval::WorkDelegations, "game_chat_work_delegations", outMetrics);
		AppendIntervalMetric(sample, EMonitorInterval::WorkSteals, "game_chat_work_steals", outMetrics);
		AppendIntervalMetric(sample, EMonitorInterval::SendBatchLimitHits, "game_net_send_batch_limit_hits", outMetrics);

		auto &counters = outMetrics.m_counters;
		auto &gauges = outMetrics.m_gauges;
		counters.push_back({"game_net_sessions_accepted_total", sample.m_accepted, {}});
		counters.push_back({"game_net_wsasend_calls_total", sample.m_sendCalls, {}});
		counters.push_back({"game_net_wsasend_pending_total", sample.m_sendPending, {}});
		gauges.push_back({"game_net_sessions", static_cast<double>(sample.m_sessions), {}});
		if (!sample.m_processName.empty())
			gauges.push_back({"game_process_info", 1.0, {{"process", sample.m_processName}}});

		constexpr const char *kCpuModes[] = {"total", "user", "kernel"};
		for (size_t index = 0; index < _countof(kCpuModes); ++index)
		{
			if (sample.m_hasHostCpu)
				gauges.push_back({"game_host_cpu_usage_ratio", sample.m_hostCpu[index], {{"mode", kCpuModes[index]}}});
			if (sample.m_hasProcessCpu)
				gauges.push_back({"game_process_cpu_usage_ratio", sample.m_processCpu[index], {{"mode", kCpuModes[index]}}});
		}

		if (sample.m_hasMemory)
		{
			const auto &memory = sample.m_memory;
			counters.push_back({"game_process_page_faults_total", sample.m_pageFaults, {}});
			gauges.push_back({"game_process_working_set_bytes", static_cast<double>(memory.WorkingSetSize), {}});
			gauges.push_back({"game_process_peak_working_set_bytes", static_cast<double>(memory.PeakWorkingSetSize), {}});
			gauges.push_back({"game_process_paged_pool_bytes", static_cast<double>(memory.QuotaPagedPoolUsage), {}});
			gauges.push_back({"game_process_peak_paged_pool_bytes", static_cast<double>(memory.QuotaPeakPagedPoolUsage), {}});
			gauges.push_back({"game_process_nonpaged_pool_bytes", static_cast<double>(memory.QuotaNonPagedPoolUsage), {}});
			gauges.push_back({"game_process_peak_nonpaged_pool_bytes", static_cast<double>(memory.QuotaPeakNonPagedPoolUsage), {}});
			gauges.push_back({"game_process_commit_bytes", static_cast<double>(memory.PagefileUsage), {}});
			gauges.push_back({"game_process_peak_commit_bytes", static_cast<double>(memory.PeakPagefileUsage), {}});
			gauges.push_back({"game_process_private_bytes", static_cast<double>(memory.PrivateUsage), {}});
		}

		// 유효하지 않은 PDH 표본은 0 대신 시계열을 생략한다.
		if (sample.m_nonpagedPoolBytes)
			gauges.push_back({"game_host_nonpaged_pool_bytes", *sample.m_nonpagedPoolBytes, {}});
		if (sample.m_availableMemoryBytes)
			gauges.push_back({"game_host_available_memory_bytes", *sample.m_availableMemoryBytes, {}});
		if (sample.m_tcpV4RetransmittedSegmentsPerSecond)
			gauges.push_back({"game_host_tcpv4_retransmitted_segments_per_second", *sample.m_tcpV4RetransmittedSegmentsPerSecond, {}});
		for (const auto &nic : sample.m_nicRates)
			gauges.push_back({"game_host_nic_total_bytes_per_second", nic.m_totalBytesPerSecond, {{"interface", nic.m_interfaceName}}});
	}
}