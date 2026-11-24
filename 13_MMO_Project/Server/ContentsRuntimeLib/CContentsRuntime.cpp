#include "ContentsRuntimeLibPch.h"
#include "IContents.h"
#include "ContentsHandle.h"
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
		std::mutex 			workLock;
		std::list<SEntry> 	entries;
		std::list<SEntry> 	delegateQueue;
		std::thread 		thread;
		bool 				isStopping = false;

		std::binary_semaphore 	wakeSignal{0};
		std::atomic<bool> 		wakePending = false;
		// 대기 전에도 신호를 남기고, 아직 소비하지 않은 신호와 합친다.
		void SignalWork() noexcept
		{
			if (!wakePending.exchange(true))
				wakeSignal.release();
		}
	};

	struct SCompletionMailbox
	{
		explicit SCompletionMailbox(const CContentsRuntime &runtime)
			: pRuntime(&runtime)
		{
		}

		// 담당 워커가 없으면 닫힌 상태다. 같은 락으로 FIFO를 보호한다.
		std::mutex 							completionLock;
		std::deque<std::function<void()>> 	completionQueue;
		const CContentsRuntime *const 		pRuntime;
		CContentsRuntime::SWorker 			*pWorker = nullptr;
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
		// mailbox의 생성을 registrationLock 이전으로 뺀다.
		auto spMailbox = std::make_shared<SCompletionMailbox>(*this);
		if (!upContents)
			return false;
		upContents->m_spMailbox = std::move(spMailbox);

		// 1. registrationLock
		std::lock_guard registrationLock(m_registrationLock);
		// #1 : 등록 가능 여부 확인
		if (m_state != EState::Registering || m_workers.empty())
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

		auto &entry = pWorker->entries.emplace_back();
		m_routes.emplace(instanceId, SRoute{&entry, pWorker});
		entry.upContents = std::move(upContents);
		entry.usesFrame = usesFrame;

		return true;
	}

	bool CContentsRuntime::Start() noexcept
	{
		// 1. registrationLock
		std::lock_guard registrationLock(m_registrationLock);
		using Clock = std::chrono::steady_clock;
		const auto now = Clock::now();
		if (m_state != EState::Registering || m_workers.empty() || m_frameInterval.count() <= 0
			|| m_frameInterval >= std::chrono::duration_cast<std::chrono::milliseconds>(Clock::time_point::max() - now))
			return false;

		for (const auto &upWorker : m_workers)
		{
			for (auto &entry : upWorker->entries)
			{
				entry.nextFrameTime = now + m_frameInterval;
				auto &mailbox = *entry.upContents->m_spMailbox;
				// 2. completionLock
				std::lock_guard completionLock(mailbox.completionLock);
				mailbox.pWorker = upWorker.get();
			}
		}

		for (const auto &upWorker : m_workers)
			upWorker->thread = std::thread(&CContentsRuntime::runWorker, this, std::ref(*upWorker));
		m_state = EState::Running;
		return true;
	}

	void CContentsRuntime::Stop()
	{
		{
			// 1. m_registrationLock
			std::lock_guard registrationLock(m_registrationLock);
			if (m_state != EState::Running)
				return;

			m_state = EState::Stopped;
			// 인계 게시를 막은 상태에서 모든 접수를 닫은 뒤 워커에 종료를 전달한다.
			for (const auto &[instanceId, route] : m_routes)
			{
				auto &mailbox = *route.pEntry->upContents->m_spMailbox;
				std::lock_guard completionLock(mailbox.completionLock);
				mailbox.pWorker = nullptr;
			}
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

	ContentsHandle CContentsRuntime::GetContentsHandle(ContentsInstanceId instanceId)
	{
		ContentsHandle handle;
		std::lock_guard registrationLock(m_registrationLock);
		if (m_state == EState::Registering)
		{
			const auto route = m_routes.find(instanceId);
			if (route != m_routes.end())
				handle.m_wpMailbox = route->second.pEntry->upContents->m_spMailbox;
		}
		return handle;
	}

	bool CContentsRuntime::PostCompletion(const ContentsHandle &handle, std::function<void()> completion)
	{
		if (!completion)
			return false;

		auto spMailbox = handle.m_wpMailbox.lock();
		if (!spMailbox)
			return false;

		// 1. completionLock
		std::lock_guard completionLock(spMailbox->completionLock);
		if (spMailbox->pWorker == nullptr || spMailbox->pRuntime != this)
			return false;

		spMailbox->completionQueue.emplace_back(std::move(completion));
		// 닫기나 인계가 담당 워커를 바꾸기 전에 신호를 남긴다.
		spMailbox->pWorker->SignalWork();
		return true;
	}

	bool CContentsRuntime::RequestDelegate(ContentsInstanceId instanceId, std::size_t destinationWorkerIndex)
	{
		// 1. m_registrationLock
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
			// 2. workLock
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
		// 1. workLock
		std::unique_lock workLock(worker.workLock);
		while (true)
		{
			// workLock이 걸린 상태에서 자신에게 온 Entry를 받는다.
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

						// 1. workLock
						workLock.lock();
						didWork = true;
					}
					if (entry.nextFrameTime < nextWakeTime)
						nextWakeTime = entry.nextFrameTime;
				}

				if (entry.pDelegateWorker == nullptr)
				{
					auto &mailbox = *entry.upContents->m_spMailbox;
					std::size_t completionCount;
					{
						// 이번 프레임에 쌓여 있던 완료만 처리하고 새로 들어온 완료는 남겨 둔다.
						std::lock_guard completionLock(mailbox.completionLock);
						completionCount = mailbox.completionQueue.size();
					}
					for (std::size_t i = 0; i < completionCount && entry.pDelegateWorker == nullptr; ++i)
					{
						std::function<void()> completion;
						{
							// 락을 걸고 완료 콜백 1개를 꺼낸다.
							std::lock_guard completionLock(mailbox.completionLock);
							completion.swap(mailbox.completionQueue.front());
							mailbox.completionQueue.pop_front();
						}
						workLock.unlock();
						// 완료 콜백은 락을 걸지 않고 수행한다.
						completion();
						completion = nullptr;
						workLock.lock();
						didWork = true;
					}
				}

				// 넘기기로 결정된 Worker를 다른 스레드로 Entry를 넘긴다.
				if (entry.pDelegateWorker != nullptr)
				{
					// 실행 경계에서 락 순서를 맞춘다. 이 노드를 옮길 수 있는 것은 현재 워커뿐이다.
					workLock.unlock();

					// 1. m_registrationLock
					std::lock_guard registrationLock(m_registrationLock);
					// 2. workLock
					workLock.lock();
					SWorker &destination = *entry.pDelegateWorker;
					if (m_state == EState::Running)
					{
						// 3. destinationWorkLock
						std::lock_guard destinationWorkLock(destination.workLock);
						auto &route = m_routes.find(entry.upContents->GetInstanceId())->second;
						auto &mailbox = *entry.upContents->m_spMailbox;
						// 4. completionLock
						std::lock_guard completionLock(mailbox.completionLock);
						// 노드와 완료 접수 경로를 같이 넘기고, 목적지가 편입할 때까지 추가 인계를 막는다.
						destination.delegateQueue.splice(destination.delegateQueue.end(), worker.entries, current);
						route.pWorker = &destination;
						mailbox.pWorker = &destination;
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