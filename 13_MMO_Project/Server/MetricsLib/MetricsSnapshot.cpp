#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "MetricsSnapshot.h"

namespace MetricsLib
{
	namespace
	{
		struct SMetricFamily
		{
			std::string_view m_type;
			std::set<std::string> m_labelSets;
			std::string m_samples;
		};

		// 매트릭의 이름과 라벨 키의 형식을 검사합니다.
		// 첫글자 숫자 불가, 한글, 공백 거부 등
		bool IsValidName(std::string_view name, bool isMetric)
		{
			if (name.empty())
			{
				return false;
			}
			for (size_t index = 0; index < name.size(); ++index)
			{
				const char ch = name[index];
				if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' || (isMetric && ch == ':') || (index != 0 && ch >= '0' && ch <= '9'))
				{
					continue;
				}
				return false;
			}
			return true;
		}

		// 숫자를 프로메테우스 응답에 넣을 형식으로 변환함.
		std::string FormatNumber(double value)
		{
			if (std::isnan(value))
			{
				return "NaN";
			}
			if (std::isinf(value))
			{
				return value < 0 ? "-Inf" : "+Inf";
			}
			// 일반 부동소수점 형식의 최대 길이보다 충분히 크다.
			char buffer[64];
			const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general);
			return std::string(buffer, result.ptr);
		}

		// 라벨 값을 프로메테우스 응답 값에 맞게 변환합니다. \n -> \\n
		void AppendEscaped(std::string& outText, std::string_view value)
		{
			for (const char ch : value)
			{
				if (ch == '\n')
				{
					outText += "\\n";
					continue;
				}
				if (ch == '\\' || ch == '"')
				{
					outText += '\\';
				}
				outText += ch;
			}
		}

		// 라벨 키를 검증하고 중복을 거른 뒤, 키 이름순으로 정렬해서 outLabels에 붙인다.
		bool FormatLabels(const std::vector<SMetricLabel>& labels, bool isHistogram, std::string& outLabels)
		{
			std::map<std::string_view, std::string_view> orderedLabels;
			for (const auto& label : labels)
			{
				if (!IsValidName(label.m_name, false) || (isHistogram && label.m_name == "le") || label.m_name.starts_with("__") || !orderedLabels.emplace(label.m_name, label.m_value).second)
				{
					return false;
				}
			}
			for (const auto& [name, value] : orderedLabels)
			{
				if (!outLabels.empty())
				{
					outLabels += ',';
				}
				outLabels += name;
				outLabels += "=\"";
				AppendEscaped(outLabels, value);
				outLabels += '"';
			}
			return true;
		}

		// 지표를 조합해 샘플 한줄을 붙인다.
		// - 샘플은 특정 지표의 관측값 한 건
		void AppendSample(std::string& outText, std::string_view name, std::string_view suffix,
			std::string_view labels, std::string_view value, std::string_view upperBound = {})
		{
			outText += name;
			outText += suffix;
			if (!labels.empty() || !upperBound.empty())
			{
				outText += '{';
				outText += labels;
				if (!upperBound.empty())
				{
					if (!labels.empty())
					{
						outText += ',';
					}
					outText += "le=\"";
					outText += upperBound;
					outText += '"';
				}
				outText += '}';
			}
			outText += ' ';
			outText += value;
			outText += '\n';
		}

		// 집계된 히스토그램의 버킷별 누적 개수, 관측값 합계, 전체 관측 개수를 Prometheus 응답에 한줄씩 붙인다.
		bool AppendHistogram(std::string& outText, const SHistogramSample& histogram, std::string_view labels)
		{
			double previousBound = -std::numeric_limits<double>::infinity();
			uint64_t previousCount = 0;
			for (const auto& bucket : histogram.m_buckets)
			{
				if (!std::isfinite(bucket.m_upperBound) || bucket.m_upperBound <= previousBound
					|| bucket.m_cumulativeCount < previousCount || bucket.m_cumulativeCount > histogram.m_count)
				{
					return false;
				}
				AppendSample(outText, histogram.m_name, "_bucket", labels, std::to_string(bucket.m_cumulativeCount),
					FormatNumber(bucket.m_upperBound));
				previousBound = bucket.m_upperBound;
				previousCount = bucket.m_cumulativeCount;
			}
			const auto count = std::to_string(histogram.m_count);
			AppendSample(outText, histogram.m_name, "_bucket", labels, count, "+Inf");
			AppendSample(outText, histogram.m_name, "_sum", labels, FormatNumber(histogram.m_sum));
			AppendSample(outText, histogram.m_name, "_count", labels, count);
			return true;
		}
	}

	// 프로메테우스 응답 본문을 만든다.
	bool FormatPrometheus(const SMetricsSnapshot& snapshot, std::string& outBody)
	{
		std::map<std::string, SMetricFamily, std::less<>> families;
		const auto addSample = [&families](const auto& sample, std::string_view type, const auto& append)
		{
			std::string labels;
			if (!IsValidName(sample.m_name, true) || !FormatLabels(sample.m_labels, type == "histogram", labels))
			{
				return false;
			}
			auto& family = families[sample.m_name];
			if ((!family.m_type.empty() && family.m_type != type) || !family.m_labelSets.insert(labels).second)
			{
				return false;
			}
			family.m_type = type;
			return append(family.m_samples, sample, labels);
		};
		for (const auto& sample : snapshot.m_counters)
		{
			if (!addSample(sample, "counter", [](std::string& outSamples, const SCounterSample& counter, std::string_view labels)
			{
				AppendSample(outSamples, counter.m_name, {}, labels, std::to_string(counter.m_value));
				return true;
			}))
			{
				return false;
			}
		}
		for (const auto& sample : snapshot.m_gauges)
		{
			if (!addSample(sample, "gauge", [](std::string& outSamples, const SGaugeSample& gauge, std::string_view labels)
			{
				AppendSample(outSamples, gauge.m_name, {}, labels, FormatNumber(gauge.m_value));
				return true;
			}))
			{
				return false;
			}
		}
		for (const auto& sample : snapshot.m_histograms)
		{
			if (!addSample(sample, "histogram", AppendHistogram))
			{
				return false;
			}
		}

		std::string body;
		for (const auto& [name, family] : families)
		{
			if (family.m_type == "histogram"
				&& (families.contains(name + "_bucket") || families.contains(name + "_sum") || families.contains(name + "_count")))
			{
				return false;
			}
			body += "# TYPE " + name + ' ';
			body += family.m_type;
			body += '\n';
			body += family.m_samples;
		}
		outBody = std::move(body);
		return true;
	}
}
