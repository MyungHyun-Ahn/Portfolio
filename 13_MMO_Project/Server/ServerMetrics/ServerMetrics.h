#pragma once

/*
// 공통 서버 수집 지표를 모아둡니다.
// - 이는 각자의 서버 프로젝트에 추가한 뒤 컴파일 합니다.
// - 서버 메모리, CPU 이용률 등을 수집합니다.
*/
namespace ServerNetworkLib
{
	enum class EMonitorInterval;
	struct SMonitorSnapshot;
}

namespace MetricsLib
{
	struct SMetricsSnapshot;
}

namespace ServerMetrics
{
	// 같은 표본에 담긴 누적값과 유효한 초당값을 추가한다.
	void AppendIntervalMetric(const ServerNetworkLib::SMonitorSnapshot &sample, ServerNetworkLib::EMonitorInterval interval, const std::string &name, MetricsLib::SMetricsSnapshot &outMetrics);
	void AppendCommonMetrics(const ServerNetworkLib::SMonitorSnapshot &sample, MetricsLib::SMetricsSnapshot &outMetrics);
}