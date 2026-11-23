#include "ContentsRuntimeLibPch.h"
#include "IContents.h"
#include "CContentsRuntime.h"

namespace ContentsRuntimeLib
{
	struct CContentsRuntime::SWorker
	{
		struct SEntry
		{
			std::unique_ptr<IContents> upContents;
			bool usesFrame;
			std::chrono::steady_clock::time_point nextFrameTime;
		};

		// 완료 큐 게시와 워커 대기·종료 상태를 같은 락으로 조율한다.
		std::mutex workLock;
		std::condition_variable workReadyCv;
		std::list<SEntry> entries;
		std::thread thread;
		bool isStopping = false;
	};

	CContentsRuntime::CContentsRuntime(std::size_t workerCount, std::chrono::milliseconds frameInterval)
		: m_frameInterval(frameInterval)
	{
		m_workers.reserve(workerCount);
		for (std::size_t i = 0; i < workerCount; ++i)
			m_workers.push_back(std::make_unique<SWorker>());
	}

	CContentsRuntime::~CContentsRuntime()
	{
		Stop();
	}

	bool CContentsRuntime::RegisterContents(std::unique_ptr<IContents> upContents, bool usesFrame,
		std::optional<std::size_t> workerIndex)
	{
		std::list<SWorker::SEntry> pendingEntry;
		std::lock_guard registrationLock(m_registrationLock);
		if (m_state != EState::Registering || !upContents || m_workers.empty())
			return false;

		const ContentsInstanceId instanceId = upContents->GetInstanceId();
		if (m_routes.contains(instanceId) || (workerIndex && *workerIndex >= m_workers.size()))
			return false;

		SWorker *pWorker = m_workers[workerIndex.value_or(0)].get();
		if (!workerIndex)
		{
			for (const auto &upWorker : m_workers)
			{
				if (upWorker->entries.size() < pWorker->entries.size())
					pWorker = upWorker.get();
			}
		}

		// 노드와 경로 등록에 필요한 할당을 마친 뒤 실행 목록에 게시한다.
		auto &entry = pendingEntry.emplace_back();
		m_routes.emplace(instanceId, SRoute{upContents.get(), pWorker});
		entry.upContents = std::move(upContents);
		entry.usesFrame = usesFrame;
		pWorker->entries.splice(pWorker->entries.end(), pendingEntry);
		return true;
	}

	bool CContentsRuntime::Start() noexcept
	{
		std::lock_guard registrationLock(m_registrationLock);
		using Clock = std::chrono::steady_clock;
		const auto now = Clock::now();
		if (m_state != EState::Registering || m_workers.empty() || m_frameInterval.count() <= 0
			|| m_frameInterval >= std::chrono::duration_cast<std::chrono::milliseconds>(Clock::time_point::max() - now))
			return false;

		for (const auto &upWorker : m_workers)
		{
			for (auto &entry : upWorker->entries)
				entry.nextFrameTime = now + m_frameInterval;
			upWorker->thread = std::thread(&CContentsRuntime::runWorker, this, std::ref(*upWorker));
		}
		m_state = EState::Running;
		return true;
	}

	void CContentsRuntime::Stop()
	{
		{
			std::lock_guard registrationLock(m_registrationLock);
			if (m_state != EState::Running)
				return;

			m_state = EState::Stopped;
			for (const auto &upWorker : m_workers)
			{
				{
					std::lock_guard workLock(upWorker->workLock);
					upWorker->isStopping = true;
				}
				upWorker->workReadyCv.notify_one();
			}
		}
		for (const auto &upWorker : m_workers)
			upWorker->thread.join();
	}

	bool CContentsRuntime::PostCompletion(ContentsInstanceId instanceId, std::function<void()> completion)
	{
		std::lock_guard registrationLock(m_registrationLock);
		if (m_state != EState::Running || !completion)
			return false;

		const auto route = m_routes.find(instanceId);
		if (route == m_routes.end())
			return false;

		const SRoute &target = route->second;
		{
			std::lock_guard workLock(target.pWorker->workLock);
			target.pContents->m_completionQueue.emplace_back(std::move(completion));
		}
		target.pWorker->workReadyCv.notify_one();
		return true;
	}

	void CContentsRuntime::runWorker(SWorker &worker)
	{
		using Clock = std::chrono::steady_clock;
		const auto frameInterval = std::chrono::duration_cast<Clock::duration>(m_frameInterval);
		std::unique_lock workLock(worker.workLock);
		while (true)
		{
			bool didWork = false;
			auto nextWakeTime = Clock::time_point::max();
			for (auto &entry : worker.entries)
			{
				if (entry.usesFrame && !worker.isStopping)
				{
					const auto now = Clock::now();
					if (entry.nextFrameTime <= now)
					{
						// 예정 시각을 한 주기만 전진시키고, 남은 프레임은 다음 순회에서 처리한다.
						entry.nextFrameTime += frameInterval;
						workLock.unlock();
						entry.upContents->OnFrame();
						workLock.lock();
						didWork = true;
					}
					if (entry.nextFrameTime < nextWakeTime)
						nextWakeTime = entry.nextFrameTime;
				}

				auto &completionQueue = entry.upContents->m_completionQueue;
				if (!completionQueue.empty())
				{
					// 순회당 최대 한 건을 꺼내고, 실행과 캡처 해제는 락 밖에서 한다.
					{
						std::function<void()> completion = std::move(completionQueue.front());
						completionQueue.pop_front();
						workLock.unlock();
						completion();
					}
					workLock.lock();
					didWork = true;
				}
			}

			if (didWork)
				continue;

			if (worker.isStopping)
				break;
			
			// 프레임 대기와 콜백을 기다리는 Cv
			if (nextWakeTime == Clock::time_point::max())
				worker.workReadyCv.wait(workLock);
			else
				worker.workReadyCv.wait_until(workLock, nextWakeTime);
		}
	}
}