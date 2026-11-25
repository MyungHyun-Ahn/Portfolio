#include "pch.h"
#include "MetricsSnapshot.h"
#include "ChatMetrics.h"

namespace Chatting
{
	namespace
	{
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

	void CChatMetrics::Collect(double elapsedSeconds, MetricsLib::SMetricsSnapshot &outMetrics)
	{
		AppendChatCounter("chat_content_updates", g_chatContentUpdates.load(), m_previousContentUpdates, elapsedSeconds, outMetrics);
		AppendChatCounter("chat_login_requests", g_chatLoginRequests.load(), m_previousLoginRequests, elapsedSeconds, outMetrics);
		AppendChatCounter("chat_sector_move_requests", g_chatSectorMoveRequests.load(), m_previousSectorMoveRequests,
			elapsedSeconds, outMetrics);
		AppendChatCounter("chat_message_requests", g_chatMessageRequests.load(), m_previousMessageRequests, elapsedSeconds, outMetrics);
		AppendChatCounter("chat_message_send_requests", g_chatMessageSendRequests.load(), m_previousMessageSendRequests,
			elapsedSeconds, outMetrics);
		outMetrics.m_counters.push_back({"chat_content_frames_total", g_completedChatFrames.load(), {}});
	}
}
