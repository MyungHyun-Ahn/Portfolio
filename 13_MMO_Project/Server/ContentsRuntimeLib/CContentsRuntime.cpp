#include "ContentsRuntimeLibPch.h"
#include "IContents.h"
#include "CContentsRuntime.h"
#include <atomic>
#include <semaphore>

namespace ContentsRuntimeLib
{
	struct CContentsRuntime::SEntry
	{
		std::unique_ptr<IContents> upContents;
		bool usesFrame;
		std::chrono::steady_clock::time_point nextFrameTime;
		SWorker *pDelegateWorker = nullptr;
	};

	struct CContentsRuntime::SWorker
	{
		// 실행 목록·인계 큐, 인계 요청과 워커 종료 상태를 보호한다.
		std::mutex workLock;
		std::list<SEntry> entries;
		std::list<SEntry> delegateQueue;
		std::thread thread;
		bool isStopping = false;

		std::binary_semaphore wakeSignal{0};
		std::atomic<bool> wakePending = false;
		// 대기 전에도 신호를 남기고, 아직 소비하지 않은 신호와 합친다.
		void SignalWork() noexcept
		{
			if (!wakePending.exchange(true))
				wakeSignal.release();
		}
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
		std::list<SEntry> pendingEntry;
		std::lock_guard registrationLock(m_registrationLock);
		// #1 : 등록 가능 여부 확인
		if (m_state != EState::Registering || !upContents || m_workers.empty())
			return false;

		// #2 : 워커 선택
		// 	- workerIndex를 전달하면 그 워커로 배정
		const ContentsInstanceId instanceId = upContents->GetInstanceId();
		if (m_routes.contains(instanceId) || (workerIndex && *workerIndex >= m_workers.size()))
			return false;

		// 	- 전달하지 않으면 entries가 가장 적은 워커로 배정
		SWorker *pWorker = m_workers[workerIndex.value_or(0)].get();
		if (!workerIndex)
		{
			for (const auto &upWorker : m_workers)
			{
				if (upWorker->entries.size() < pWorker->entries.size())
					pWorker = upWorker.get();
			}
		}

		// #3 : 노드와 경로 등록에 필요한 할당을 마친 뒤 실행 목록에 게시한다.
		auto &entry = pendingEntry.emplace_back();
		m_routes.emplace(instanceId, SRoute{&entry, pWorker});
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
				upWorker->SignalWork();
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
		auto &contents = *target.pEntry->upContents;
		{
			// 완료 루틴을 예약한다.
			std::lock_guard completionLock(contents.m_completionLock);
			contents.m_completionQueue.emplace_back(std::move(completion));
		}

		target.pWorker->SignalWork();
		return true;
	}

	bool CContentsRuntime::RequestDelegate(ContentsInstanceId instanceId, std::size_t destinationWorkerIndex)
	{
		std::lock_guard registrationLock(m_registrationLock);
		if (m_state != EState::Running || destinationWorkerIndex >= m_workers.size())
			return false;

		const auto route = m_routes.find(instanceId);
		if (route == m_routes.end())
			return false;

		const SRoute &target = route->second;
		SWorker *pDestination = m_workers[destinationWorkerIndex].get();
		if (target.pWorker == pDestination)
			return false;
		{
			std::lock_guard workLock(target.pWorker->workLock);
			if (target.pEntry->pDelegateWorker != nullptr)
				return false;
			target.pEntry->pDelegateWorker = pDestination;
		}
		target.pWorker->SignalWork();
		return true;
	}

	void CContentsRuntime::runWorker(SWorker &worker)
	{
		using Clock = std::chrono::steady_clock;
		const auto frameInterval = std::chrono::duration_cast<Clock::duration>(m_frameInterval);
		std::unique_lock workLock(worker.workLock);
		while (true)
		{
			// 게시된 엔트리는 종료 중에도 편입해서 접수한 완료를 처리한다.
			for (auto &entry : worker.delegateQueue)
				entry.pDelegateWorker = nullptr;
			worker.entries.splice(worker.entries.end(), worker.delegateQueue);

			bool didWork = false;
			auto nextWakeTime = Clock::time_point::max();
			for (auto next = worker.entries.begin(); next != worker.entries.end();)
			{
				const auto current = next++;
				auto &entry = *current;
				if (entry.pDelegateWorker == nullptr && entry.usesFrame && !worker.isStopping)
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

				if (entry.pDelegateWorker == nullptr)
				{
					std::function<void()> completion;
					{
						auto &contents = *entry.upContents;
						std::lock_guard completionLock(contents.m_completionLock);
						if (!contents.m_completionQueue.empty())
						{
							completion = std::move(contents.m_completionQueue.front());
							contents.m_completionQueue.pop_front();
						}
					}
					// 순회당 최대 한 건을 락 밖에서 실행하고 캡처도 해제한다.
					if (completion)
					{
						workLock.unlock();
						completion();
						completion = nullptr;
						workLock.lock();
						didWork = true;
					}
				}

				if (entry.pDelegateWorker != nullptr)
				{
					// 실행 경계에서 락 순서를 맞춘다. 이 노드를 옮길 수 있는 것은 현재 워커뿐이다.
					workLock.unlock();
					std::lock_guard registrationLock(m_registrationLock);
					workLock.lock();
					SWorker &destination = *entry.pDelegateWorker;
					if (m_state == EState::Running)
					{
						std::lock_guard destinationWorkLock(destination.workLock);
						auto &route = m_routes.find(entry.upContents->GetInstanceId())->second;
						// 노드와 완료 접수 경로를 같이 넘기고, 목적지가 편입할 때까지 추가 인계를 막는다.
						destination.delegateQueue.splice(destination.delegateQueue.end(), worker.entries, current);
						route.pWorker = &destination;
						destination.SignalWork();
					}
					else
					{
						entry.pDelegateWorker = nullptr;
					}
					// 락을 놓은 사이에 들어온 작업과 취소 후 남은 완료 큐를 다시 확인한다.
					didWork = true;
				}
			}

			if (didWork)
				continue;

			if (worker.isStopping)
				break;

			// 신호를 실제로 소비한 뒤에만 새 깨움을 허용하고 다시 순회한다.
			workLock.unlock();
			if (nextWakeTime == Clock::time_point::max())
			{
				worker.wakeSignal.acquire();
				worker.wakePending = false;
			}
			else if (worker.wakeSignal.try_acquire_until(nextWakeTime))
			{
				worker.wakePending = false;
			}
			workLock.lock();
		}
	}
}