#pragma once

namespace MetricsLib
{
	struct SMetricsSnapshot;
}

namespace Chatting
{
	[[nodiscard]]
	std::function<bool(MetricsLib::SMetricsSnapshot &)> MakeChatMetricsCollector();
}
