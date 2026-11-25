#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "MetricsSnapshot.h"
#include "CMetricsExporter.h"

#include <httplib.h>
#include <pdh.h>
#include <psapi.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "CSystemMetrics.h"

namespace
{
	void Check(bool isTrue, const char* pMessage)
	{
		if (!isTrue)
		{
			std::fputs(pMessage, stderr);
			std::fputc('\n', stderr);
			std::exit(EXIT_FAILURE);
		}
	}

	constexpr char kPublishTimeType[] = "# TYPE observability_snapshot_timestamp_seconds gauge\n";
	constexpr char kPublishTimeSample[] = "observability_snapshot_timestamp_seconds ";

	std::string StripPublishTime(std::string body, double *pPublishTime = nullptr)
	{
		const auto start = body.find(kPublishTimeType);
		Check(start != std::string::npos, "publication timestamp gauge missing");
		const auto sample = start + sizeof(kPublishTimeType) - 1;
		Check(body.compare(sample, sizeof(kPublishTimeSample) - 1, kPublishTimeSample) == 0, "publication timestamp sample missing");
		const auto value = sample + sizeof(kPublishTimeSample) - 1;
		const auto end = body.find('\n', value);
		Check(end != std::string::npos, "publication timestamp line is incomplete");
		if (pPublishTime != nullptr)
		{
			*pPublishTime = std::stod(body.substr(value, end - value));
		}
		body.erase(start, end + 1 - start);
		return body;
	}

	double UnixSecondsNow()
	{
		return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	// 호스트·프로세스 지표가 기존 형식으로 추가되는지 확인한다.
	void AppendsCurrentProcessMetrics()
	{
		MetricsLib::CSystemMetrics collector;
		MetricsLib::SMetricsSnapshot snapshot;
		snapshot.m_counters.push_back({"test_requests_total", 1, {}});
		collector.Collect(snapshot);

		std::string body;
		Check(MetricsLib::FormatPrometheus(snapshot, body), "host/process metrics must format as Prometheus metrics");
		Check(body.find("process_info{process=\"MetricsTests.exe\"} 1") != std::string::npos,
			"process info must contain only the executable basename");
		Check(body.find("# TYPE process_page_faults_total counter") != std::string::npos, "process page faults missing");
		Check(body.find("# TYPE process_working_set_bytes gauge") != std::string::npos, "process working set missing");
		Check(body.find("test_requests_total 1") != std::string::npos, "collection must append to the caller's snapshot");
		Check(body.find("host_nic_total_bytes_per_second") == std::string::npos, "first sample must omit NIC rates");
	}

	// Windows가 직접 보고한 프로세스의 최대 작업 집합을 바이트로 읽는다.
	SIZE_T ReadPeakWorkingSetBytes()
	{
		PROCESS_MEMORY_COUNTERS memory{};
		memory.cb = sizeof(memory);
		Check(GetProcessMemoryInfo(GetCurrentProcess(), &memory, memory.cb) != FALSE, "could not read Windows process memory");
		return memory.PeakWorkingSetSize;
	}

	// 이름과 필요 시 mode 라벨이 일치하는 게이지를 찾는다.
	double FindGauge(const MetricsLib::SMetricsSnapshot &snapshot, const char *pName, const char *pMode = nullptr)
	{
		for (const auto &gauge : snapshot.m_gauges)
		{
			if (gauge.m_name != pName)
				continue;
			if (pMode == nullptr || (gauge.m_labels.size() == 1 && gauge.m_labels[0].m_name == "mode"
				&& gauge.m_labels[0].m_value == pMode))
				return gauge.m_value;
		}
		Check(false, pName);
		return 0.0;
	}

	// CPU 게이지의 단위가 비율이며 총합이 사용자·커널 시간과 일치하는지 확인한다.
	void CheckCpuRatios(const MetricsLib::SMetricsSnapshot &snapshot, const char *pName)
	{
		const double total = FindGauge(snapshot, pName, "total");
		const double user = FindGauge(snapshot, pName, "user");
		const double kernel = FindGauge(snapshot, pName, "kernel");
		Check(std::isfinite(total) && total >= 0.0 && total <= 1.0 && user >= 0.0 && kernel >= 0.0,
			"CPU samples must be finite nonnegative ratios");
		Check(std::abs(total - (user + kernel)) < 1e-9, "CPU total must equal user plus kernel");
	}

	// 표본 간 간격을 둬 CPU·PDH 및 NIC 구간 값이 나오는지 확인한다.
	void CollectsCpuAndPdhAfterWarmup()
	{
		MetricsLib::CSystemMetrics collector;
		MetricsLib::SMetricsSnapshot first;
		collector.Collect(first);
		std::this_thread::sleep_for(std::chrono::milliseconds(1100));

		const auto peakBefore = ReadPeakWorkingSetBytes();
		MetricsLib::SMetricsSnapshot second;
		collector.Collect(second);
		const auto peakAfter = ReadPeakWorkingSetBytes();
		const double peakBytes = FindGauge(second, "process_peak_working_set_bytes");
		const double workingSetBytes = FindGauge(second, "process_working_set_bytes");
		Check(peakBytes >= static_cast<double>(peakBefore) && peakBytes <= static_cast<double>(peakAfter)
			&& workingSetBytes > 0 && workingSetBytes <= peakBytes, "process memory must match Windows byte units");
		CheckCpuRatios(second, "host_cpu_usage_ratio");
		CheckCpuRatios(second, "process_cpu_usage_ratio");

		const double availableBytes = FindGauge(second, "host_available_memory_bytes");
		constexpr double kBytesPerMegabyte = 1024.0 * 1024.0;
		Check(availableBytes >= kBytesPerMegabyte && std::fmod(availableBytes, kBytesPerMegabyte) == 0.0,
			"PDH available memory must be converted from megabytes to bytes");
		Check(FindGauge(second, "host_nonpaged_pool_bytes") > 0, "PDH nonpaged pool sample missing or invalid");
		Check(FindGauge(second, "host_tcpv4_retransmitted_segments_per_second") >= 0, "PDH TCP sample is invalid");

		bool hasNic = false;
		for (const auto &gauge : second.m_gauges)
		{
			if (gauge.m_name != "host_nic_total_bytes_per_second")
				continue;
			Check(gauge.m_labels.size() == 1 && gauge.m_labels[0].m_name == "interface" && !gauge.m_labels[0].m_value.empty()
				&& std::isfinite(gauge.m_value) && gauge.m_value >= 0.0, "invalid PDH NIC sample");
			hasNic = true;
		}
		Check(hasNic, "PDH NIC samples missing after warm-up");
		std::string body;
		Check(MetricsLib::FormatPrometheus(second, body), "second system sample must format as Prometheus metrics");
	}

	// 프로세스 조회가 실패해도 수집할 수 있는 호스트 지표는 남긴다.
	void OmitsUnavailableProcessMetrics()
	{
		MetricsLib::CSystemMetrics collector(nullptr);
		MetricsLib::SMetricsSnapshot snapshot;
		collector.Collect(snapshot);
		Check(snapshot.m_counters.empty(), "failed process sample must not report page faults as zero");
		for (const auto &gauge : snapshot.m_gauges)
			Check(gauge.m_name.starts_with("host_"), "failed process sample must omit process gauges");
		Check(FindGauge(snapshot, "host_available_memory_bytes") > 0, "process collection failure must preserve host PDH metrics");
	}

	void ReturnsUnavailableBeforePublication()
	{
		MetricsLib::CMetricsExporter exporter;
		uint16_t boundPort = 0;
		Check(exporter.Start("127.0.0.1", 0, boundPort) && boundPort != 0, "failed to start a loopback exporter");

		httplib::Client client("127.0.0.1", boundPort);
		client.set_read_timeout(2, 0);
		const auto response = client.Get("/metrics");
		Check(response && response->status == 503, "unpublished endpoint must return HTTP 503");
	}

	void BindsOnlyToTheRequestedAddress()
	{
		MetricsLib::CMetricsExporter exporter;
		uint16_t boundPort = 0;
		Check(!exporter.Start("", 0, boundPort) && boundPort == 0, "empty bind address must be rejected");
		Check(exporter.Start("127.0.0.2", 0, boundPort) && boundPort != 0, "could not bind the requested alternate address");
		httplib::Client client("127.0.0.2", boundPort);
		client.set_read_timeout(2, 0);
		const auto response = client.Get("/metrics");
		Check(response && response->status == 503, "the requested address must serve /metrics");
		httplib::Client otherAddress("127.0.0.1", boundPort);
		otherAddress.set_connection_timeout(1, 0);
		Check(!otherAddress.Get("/metrics"), "the exporter must not listen on the default address as well");
	}

	void ReturnsLatestPublishedCounter()
	{
		MetricsLib::CMetricsExporter exporter;
		uint16_t boundPort = 0;
		Check(exporter.Start("127.0.0.1", 0, boundPort), "failed to start exporter for publication");
		uint64_t value = 9007199254740993;
		int calls = 0;
		bool canCollect = true;
		const auto publish = exporter.CreatePublishCallback([&](MetricsLib::SMetricsSnapshot& outSnapshot)
		{
			++calls;
			if (!canCollect)
			{
				return false;
			}
			outSnapshot.m_counters.push_back({"test_requests_total", value, {}});
			return true;
		});
		Check(calls == 0, "creating a callback must not collect");

		httplib::Client client("127.0.0.1", boundPort);
		client.set_read_timeout(2, 0);
		const double before = UnixSecondsNow();
		publish();
		const double after = UnixSecondsNow();
		const auto first = client.Get("/metrics");
		Check(first && first->status == 200, "publication must make the endpoint available");
		Check(first->get_header_value("Content-Type") == "text/plain; version=0.0.4; charset=utf-8", "wrong metrics content type");
		double firstPublishTime = 0.0;
		Check(StripPublishTime(first->body, &firstPublishTime) == "# TYPE test_requests_total counter\ntest_requests_total 9007199254740993\n",
			"counter lost precision");
		Check(firstPublishTime >= before && firstPublishTime <= after, "publication time must be Unix seconds at publication");
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		const auto repeated = client.Get("/metrics");
		Check(repeated && repeated->body == first->body, "scraping must not update publication time");
		++value;
		publish();
		const auto second = client.Get("/metrics");
		double secondPublishTime = 0.0;
		Check(second && StripPublishTime(second->body, &secondPublishTime)
			== "# TYPE test_requests_total counter\ntest_requests_total 9007199254740994\n",
			"second publication must replace the old snapshot");
		Check(secondPublishTime > firstPublishTime, "a new publication must update the publication time");
		canCollect = false;
		publish();
		const auto third = client.Get("/metrics");
		Check(third && third->status == 200 && third->body == second->body, "failed collection must preserve the latest snapshot");
		Check(calls == 3, "scrapes must not invoke the collector");
	}

	void FormatsLabeledMetricsAndRejectsInvalidHistograms()
	{
		MetricsLib::CMetricsExporter exporter;
		uint16_t boundPort = 0;
		Check(exporter.Start("127.0.0.1", 0, boundPort), "failed to start exporter for format test");
		uint64_t count = 4;
		const auto publish = exporter.CreatePublishCallback([&](MetricsLib::SMetricsSnapshot& outSnapshot)
		{
			outSnapshot.m_counters = {{"requests_total", 1, {{"result", "ok"}}}, {"requests_total", 2, {{"result", "error"}}}};
			outSnapshot.m_gauges = {{"queue_depth", 1.25, {{"name", "a\\b\"c\nd"}}}};
			outSnapshot.m_histograms = {{"elapsed_seconds", count, 2.5, {{0.25, 1}, {1, 3}}, {{"operation", "send"}}}};
			return true;
		});
		publish();
		httplib::Client client("127.0.0.1", boundPort);
		client.set_read_timeout(2, 0);
		const auto response = client.Get("/metrics");
		const std::string expected =
			"# TYPE elapsed_seconds histogram\n"
			"elapsed_seconds_bucket{operation=\"send\",le=\"0.25\"} 1\n"
			"elapsed_seconds_bucket{operation=\"send\",le=\"1\"} 3\n"
			"elapsed_seconds_bucket{operation=\"send\",le=\"+Inf\"} 4\n"
			"elapsed_seconds_sum{operation=\"send\"} 2.5\n"
			"elapsed_seconds_count{operation=\"send\"} 4\n"
			"# TYPE queue_depth gauge\nqueue_depth{name=\"a\\\\b\\\"c\\nd\"} 1.25\n"
			"# TYPE requests_total counter\nrequests_total{result=\"ok\"} 1\nrequests_total{result=\"error\"} 2\n";
		Check(response && response->status == 200 && StripPublishTime(response->body) == expected, "wrong Prometheus exposition");
		count = 2;
		publish();
		const auto afterInvalid = client.Get("/metrics");
		Check(afterInvalid && afterInvalid->body == response->body, "invalid histogram must preserve the previous snapshot");
	}

	void RejectsPortCollisionAndReleasesListenerOnStop()
	{
		MetricsLib::CMetricsExporter owner;
		MetricsLib::CMetricsExporter contender;
		uint16_t boundPort = 0;
		Check(owner.Start("127.0.0.1", 0, boundPort), "could not reserve collision port");
		uint16_t contenderPort = 0;
		Check(!contender.Start("127.0.0.1", boundPort, contenderPort) && contenderPort == 0, "occupied port must fail synchronously");
		Check(!owner.Start("127.0.0.1", 0, contenderPort) && contenderPort == 0, "start must not replace an active listener");
		owner.Stop();
		owner.Stop();
		Check(contender.Start("127.0.0.1", boundPort, contenderPort) && contenderPort == boundPort, "Stop must release the listener");

		httplib::Client client("127.0.0.1", contenderPort);
		client.set_read_timeout(2, 0);
		const auto notFound = client.Get("/other");
		Check(notFound && notFound->status == 404, "only the metrics route should be registered");
		contender.Stop();
		const auto stopped = client.Get("/metrics");
		Check(!stopped, "a stopped exporter must not accept scrapes");
	}

	void ConcurrentScrapesSeeCompleteSnapshots()
	{
		MetricsLib::CMetricsExporter exporter;
		uint16_t boundPort = 0;
		Check(exporter.Start("127.0.0.1", 0, boundPort), "could not start concurrent scrape fixture");
		uint64_t version = 1;
		const auto publish = exporter.CreatePublishCallback([&](MetricsLib::SMetricsSnapshot& outSnapshot)
		{
			outSnapshot.m_counters = {{"a_total", version, {}}, {"b_total", version, {}}};
			return true;
		});
		publish();
		const auto makeBody = [](const char* pValue)
		{
			return std::string("# TYPE a_total counter\na_total ") + pValue + "\n# TYPE b_total counter\nb_total " + pValue + '\n';
		};
		const auto firstBody = makeBody("1");
		const auto secondBody = makeBody("2");
		std::barrier startBarrier(3);
		std::atomic<int> readersDone = 0;
		const auto scrape = [&]
		{
			httplib::Client client("127.0.0.1", boundPort);
			client.set_read_timeout(2, 0);
			startBarrier.arrive_and_wait();
			for (int index = 0; index < 32; ++index)
			{
				const auto response = client.Get("/metrics");
				Check(response && response->status == 200, "concurrent scrape failed");
				const auto body = StripPublishTime(response->body);
				Check(body == firstBody || body == secondBody,
					"concurrent response must match one complete publication");
			}
			++readersDone;
		};
		std::jthread firstReader(scrape);
		std::jthread secondReader(scrape);
		startBarrier.arrive_and_wait();
		do
		{
			version = 3 - version;
			publish();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		} while (readersDone != 2);
	}
}

int main(void)
{
	AppendsCurrentProcessMetrics();
	CollectsCpuAndPdhAfterWarmup();
	OmitsUnavailableProcessMetrics();
	BindsOnlyToTheRequestedAddress();
	ReturnsUnavailableBeforePublication();
	ReturnsLatestPublishedCounter();
	FormatsLabeledMetricsAndRejectsInvalidHistograms();
	RejectsPortCollisionAndReleasesListenerOnStop();
	ConcurrentScrapesSeeCompleteSnapshots();
	return EXIT_SUCCESS;
}
