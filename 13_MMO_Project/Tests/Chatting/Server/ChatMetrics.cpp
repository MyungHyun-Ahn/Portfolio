#include "pch.h"
#include "CMonitor.h"
#include "MetricsSnapshot.h"
#include "../../../Server/ServerMetrics/ServerMetrics.h"
#include "ChatMetrics.h"

namespace Chatting
{
	namespace
	{
		struct SChatCounts
		{
			std::uint64_t m_contentUpdates = 0;
			std::uint64_t m_loginRequests = 0;
			std::uint64_t m_sectorMoveRequests = 0;
			std::uint64_t m_messageRequests = 0;
			std::uint64_t m_messageSendRequests = 0;
		};

		std::atomic<std::uint64_t> g_chatContentUpdates{0};
		std::atomic<std::uint64_t> g_chatLoginRequests{0};
		std::atomic<std::uint64_t> g_chatSectorMoveRequests{0};
		std::atomic<std::uint64_t> g_chatMessageRequests{0};
		std::atomic<std::uint64_t> g_chatMessageSendRequests{0};
		std::atomic<std::uint64_t> g_completedChatFrames{0};

		// 첫 표본에는 누계만 추가하고 다음 표본부터 실제 경과 시간으로 처리율을 계산한다.
		void AppendChatCounter(const std::string &name, std::uint64_t total, std::uint64_t &previousTotal, double elapsedSeconds,
			MetricsLib::SMetricsSnapshot &outMetrics)
		{
			outMetrics.m_counters.push_back({name + "_total", total, {}});
			if (elapsedSeconds > 0.0)
				outMetrics.m_gauges.push_back({name + "_per_second", static_cast<double>(total - previousTotal) / elapsedSeconds, {}});
			previousTotal = total;
		}
	}

	// 콘텐츠가 처리하기 시작한 수신 패킷을 센다.
	void RecordChatContentUpdate() noexcept
	{
		++g_chatContentUpdates;
	}

	// 디스패치가 확인한 로그인 요청을 센다.
	void RecordChatLoginRequest() noexcept
	{
		++g_chatLoginRequests;
	}

	// 디스패치가 확인한 섹터 이동 요청을 센다.
	void RecordChatSectorMoveRequest() noexcept
	{
		++g_chatSectorMoveRequests;
	}

	// 디스패치가 확인한 채팅 메시지 요청을 센다.
	void RecordChatMessageRequest() noexcept
	{
		++g_chatMessageRequests;
	}

	// 메시지 응답과 피어 알림의 송신 요청을 센다.
	void RecordChatMessageSendRequest() noexcept
	{
		++g_chatMessageSendRequests;
	}

	void RecordCompletedChatFrame() noexcept
	{
		++g_completedChatFrames;
	}

	std::function<bool(MetricsLib::SMetricsSnapshot &)> MakeChatMetricsCollector()
	{
		return [lastSampleMilliseconds = ULONGLONG{0}, nextCheck = std::chrono::steady_clock::time_point{},
			lastChatSample = std::chrono::steady_clock::time_point{}, previous = SChatCounts{}]
			(MetricsLib::SMetricsSnapshot &outMetrics) mutable
		{
			const auto now = std::chrono::steady_clock::now();
			if (now < nextCheck)
				return false;
			nextCheck = now + std::chrono::milliseconds(100);

			const auto spSample = ServerNetworkLib::g_monitor.GetSnapshot();
			if (!spSample || lastSampleMilliseconds == spSample->m_sampledAtMilliseconds)
				return false;
			const auto &sample = *spSample;
			lastSampleMilliseconds = sample.m_sampledAtMilliseconds;

			// 공용 표본과 함께 게시하되 채팅 카운터를 읽은 시각은 따로 기록한다.
			ServerMetrics::AppendCommonMetrics(sample, outMetrics);
			const auto chatSampleTime = std::chrono::steady_clock::now();
			const double elapsedSeconds = lastChatSample == std::chrono::steady_clock::time_point{} ? 0.0
				: std::chrono::duration<double>(chatSampleTime - lastChatSample).count();
			lastChatSample = chatSampleTime;
			AppendChatCounter("chat_content_updates", g_chatContentUpdates.load(), previous.m_contentUpdates, elapsedSeconds, outMetrics);
			AppendChatCounter("chat_login_requests", g_chatLoginRequests.load(), previous.m_loginRequests, elapsedSeconds, outMetrics);
			AppendChatCounter("chat_sector_move_requests", g_chatSectorMoveRequests.load(), previous.m_sectorMoveRequests,
				elapsedSeconds, outMetrics);
			AppendChatCounter("chat_message_requests", g_chatMessageRequests.load(), previous.m_messageRequests,
				elapsedSeconds, outMetrics);
			AppendChatCounter("chat_message_send_requests", g_chatMessageSendRequests.load(), previous.m_messageSendRequests,
				elapsedSeconds, outMetrics);
			outMetrics.m_counters.push_back({"chat_content_frames_total", g_completedChatFrames.load(), {}});

			return true;
		};
	}
}
