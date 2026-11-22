#pragma once

namespace MetricsLib
{
	// 지표를 결과나 동작 종류 등으로 구분하는 이름과 값 한 쌍이다.
	struct SMetricLabel
	{
		std::string m_name;
		std::string m_value;
	};

	// 요청 처리 건수처럼 계속 누적되는 카운터의 현재 값을 담는다.
	struct SCounterSample
	{
		std::string 				m_name;
		uint64_t 					m_value;
		std::vector<SMetricLabel> 	m_labels;
	};

	// 활성 세션 수처럼 오르내릴 수 있는 지표의 현재 값을 담는다.
	struct SGaugeSample
	{
		std::string 				m_name;
		double 						m_value;
		std::vector<SMetricLabel> 	m_labels;
	};

	// 지정한 상한 이하에 해당하는 관측값의 누적 개수를 담는다.
	struct SHistogramBucket
	{
		double 			m_upperBound;
		uint64_t 		m_cumulativeCount;
	};

	// 히스토그램 하나의 전체 관측 개수·합계와 상한별 누적 개수를 담는다.
	struct SHistogramSample
	{
		std::string 					m_name;
		uint64_t 						m_count;
		double 							m_sum;
		// 유한 상한을 오름차순으로 제공한다. +Inf 버킷은 m_count에서 생성한다.
		std::vector<SHistogramBucket> 	m_buckets;
		std::vector<SMetricLabel> 		m_labels;
	};

	// 한 번에 게시할 카운터·게이지·히스토그램 지표를 모아 둔다.
	struct SMetricsSnapshot
	{
		std::vector<SCounterSample> 		m_counters;
		std::vector<SGaugeSample> 			m_gauges;
		std::vector<SHistogramSample> 		m_histograms;
	};

	// MetricsLib의 사용자는 이 함수를 직접 호출할 일이 없다.
	// - CreatePublishCallback()이 반환한 publish()를 실행하면,
	// - 그 콜백 내부에서 FormatPrometheus()를 호출함.
	[[nodiscard]] 
	bool FormatPrometheus(const SMetricsSnapshot& snapshot, std::string& outBody);
}
