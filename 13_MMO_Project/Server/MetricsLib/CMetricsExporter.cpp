#include <httplib.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>
#include "MetricsSnapshot.h"
#include "CMetricsExporter.h"

namespace MetricsLib
{
	struct CMetricsExporter::SListener
	{
		~SListener()
		{
			m_server.stop();
			if (m_listenerThread.joinable())
			{
				m_listenerThread.join();
			}
		}

		httplib::Server m_server;
		std::thread m_listenerThread;
	};

	CMetricsExporter::CMetricsExporter() noexcept = default;
	CMetricsExporter::~CMetricsExporter() = default;

	void CMetricsExporter::Stop() noexcept
	{
		m_pListener.reset();
	}

	std::function<void()> CMetricsExporter::CreatePublishCallback(std::function<bool(SMetricsSnapshot&)> collector)
	{
		return [this, collector = std::move(collector)]
		{
			SMetricsSnapshot snapshot;
			if (!collector || !collector(snapshot))
			{
				return;
			}
			const auto now = std::chrono::system_clock::now();
			const double unixSeconds = std::chrono::duration<double>(now.time_since_epoch()).count();
			snapshot.m_gauges.push_back({"game_observability_snapshot_timestamp_seconds", unixSeconds, {}});

			std::string body;
			if (!FormatPrometheus(snapshot, body))
			{
				return;
			}
			m_spLatestBody.store(std::make_shared<const std::string>(std::move(body)), std::memory_order_release);
		};
	}

	bool CMetricsExporter::Start(const std::string& bindAddress, uint16_t port, uint16_t& outBoundPort)
	{
		if (m_pListener || bindAddress.empty() || bindAddress.find(char{}) != std::string::npos)
		{
			return false;
		}

		auto pListener = std::make_unique<SListener>();
		auto& server = pListener->m_server;
		server.set_socket_options([](socket_t socket)
		{
			httplib::set_socket_opt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, 1);
		});
		server.new_task_queue = []
		{
			return new httplib::ThreadPool(2, 2, 16);
		};
		server.set_read_timeout(2).set_write_timeout(2).set_keep_alive_max_count(1);
		server.Get("/metrics", [pLatestBody = &m_spLatestBody](const httplib::Request&, httplib::Response& response)
		{
			const auto spBody = pLatestBody->load(std::memory_order_acquire);
			if (!spBody)
			{
				response.status = 503;
				return;
			}
			response.set_content(*spBody, "text/plain; version=0.0.4; charset=utf-8");
		});

		const int boundPort = port == 0 ? server.bind_to_any_port(bindAddress)
			: (server.bind_to_port(bindAddress, port) ? port : -1);
		if (boundPort <= 0)
		{
			return false;
		}

		try
		{
			// 소켓은 이미 listen 상태이므로 스레드 시작을 별도로 기다리지 않는다.
			pListener->m_listenerThread = std::thread([pServer = &server]
			{
				pServer->listen_after_bind();
			});
		}
		catch (const std::system_error&)
		{
			return false;
		}

		m_pListener = std::move(pListener);
		outBoundPort = static_cast<uint16_t>(boundPort);
		return true;
	}
}
