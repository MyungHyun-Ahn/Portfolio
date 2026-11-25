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

	[[nodiscard]]
	std::function<bool(MetricsLib::SMetricsSnapshot &)> MakeChatMetricsCollector();
}
