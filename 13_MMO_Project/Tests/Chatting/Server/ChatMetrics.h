#pragma once

namespace MetricsLib
{
	struct SMetricsSnapshot;
}

namespace Chatting
{
	void RecordChatContentUpdate() noexcept;
	void RecordChatLoginRequest() noexcept;
	void RecordChatSectorMoveRequest() noexcept;
	void RecordChatMessageRequest() noexcept;
	void RecordChatMessageSendRequest() noexcept;
	void RecordCompletedChatFrame() noexcept;

	class CChatMetrics
	{
	public:
		// 로그 스레드에서 이전 채팅 누계와 비교해 처리율을 계산한다.
		void Collect(double elapsedSeconds, MetricsLib::SMetricsSnapshot &outMetrics);

	private:
		std::uint64_t m_previousContentUpdates = 0;
		std::uint64_t m_previousLoginRequests = 0;
		std::uint64_t m_previousSectorMoveRequests = 0;
		std::uint64_t m_previousMessageRequests = 0;
		std::uint64_t m_previousMessageSendRequests = 0;
	};
}
