#include "pch.h"
#include "CMonitor.h"
#include "MetricsSnapshot.h"
#include "../../../Server/ServerMetrics/ServerMetrics.h"
#include "ChatMetrics.h"

namespace Chatting
{
	namespace
	{
		std::atomic<std::uint64_t> g_completedChatFrames{0};
	}

	void RecordCompletedChatFrame() noexcept
	{
		++g_completedChatFrames;
	}

	std::function<bool(MetricsLib::SMetricsSnapshot &)> MakeChatMetricsCollector()
	{
		return [lastSampleMilliseconds = ULONGLONG{0}, nextCheck = std::chrono::steady_clock::time_point{}]
			(MetricsLib::SMetricsSnapshot &outMetrics) mutable
		{
			const auto now = std::chrono::steady_clock::now();
			if (now < nextCheck)
				return false;
			nextCheck = now + std::chrono::milliseconds(100);

			const auto spSample = NetworkLib::g_monitor.GetSnapshot();
			if (!spSample || lastSampleMilliseconds == spSample->m_sampledAtMilliseconds)
				return false;
			const auto &sample = *spSample;
			lastSampleMilliseconds = sample.m_sampledAtMilliseconds;

			// 여기에 chat server 독립적인 수집 지표를 작성한다.
			using NetworkLib::EMonitorInterval;
			using ServerMetrics::AppendIntervalMetric;
			ServerMetrics::AppendCommonMetrics(sample, outMetrics);
			AppendIntervalMetric(sample, EMonitorInterval::ContentUpdates, "game_chat_content_updates", outMetrics);
			AppendIntervalMetric(sample, EMonitorInterval::LoginRequests, "game_chat_login_requests", outMetrics);
			AppendIntervalMetric(sample, EMonitorInterval::SectorMoveRequests, "game_chat_sector_move_requests", outMetrics);
			AppendIntervalMetric(sample, EMonitorInterval::MessageRequests, "game_chat_message_requests", outMetrics);
			AppendIntervalMetric(sample, EMonitorInterval::MessageSendRequests, "game_chat_message_send_requests", outMetrics);
			outMetrics.m_counters.push_back({"game_chat_content_frames_total", g_completedChatFrames.load(), {}});

			return true;
		};
	}
}
