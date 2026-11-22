#pragma once

namespace MetricsLib
{
	struct SMetricsSnapshot;
}

namespace Chatting
{
	void RecordCompletedChatFrame() noexcept;

	[[nodiscard]]
	std::function<bool(MetricsLib::SMetricsSnapshot &)> MakeChatMetricsCollector();
}
