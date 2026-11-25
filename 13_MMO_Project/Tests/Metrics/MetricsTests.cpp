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
	BindsOnlyToTheRequestedAddress();
	ReturnsUnavailableBeforePublication();
	ReturnsLatestPublishedCounter();
	FormatsLabeledMetricsAndRejectsInvalidHistograms();
	RejectsPortCollisionAndReleasesListenerOnStop();
	ConcurrentScrapesSeeCompleteSnapshots();
	return EXIT_SUCCESS;
}
