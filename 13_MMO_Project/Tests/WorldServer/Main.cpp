#include "pch.h"
#include "IContents.h"
#include "CContentsRuntime.h"

namespace WorldServer
{
	using namespace std::chrono_literals;
	using ContentsRuntimeLib::CContentsRuntime;
	using ContentsRuntimeLib::ContentsInstanceId;
	using ContentsRuntimeLib::IContents;
	constexpr auto kWaitTimeout = 3s;

	class CTestContents final : public IContents
	{
	public:
		// 테스트 콘텐츠의 식별자와 첫 프레임 대기 여부를 지정한다.
		CTestContents(ContentsInstanceId instanceId, bool isFirstFrameHeld)
			: IContents(instanceId)
			, m_isFirstFrameHeld(isFirstFrameHeld)
		{
		}

		// 담당 워커의 완료 콜백에서 실행 이력을 기록한다.
		void RecordCompletion(int sequence)
		{
			if (m_activeCount.fetch_add(1) != 0)
			{
				m_didOverlap = true;
				--m_activeCount;
				return;
			}
			m_completionThread = std::this_thread::get_id();
			m_completionThreads.push_back(m_completionThread);
			m_completionFrameCounts.push_back(m_frameCount);
			m_completed.push_back(sequence);
			--m_activeCount;
		}

	protected:
		// 첫 프레임에서만 워커를 멈춰 완료 콜백과의 경쟁을 재현한다.
		void OnFrame() override
		{
			if (m_activeCount.fetch_add(1) != 0)
			{
				m_didOverlap = true;
				--m_activeCount;
				return;
			}
			m_frameThread = std::this_thread::get_id();
			m_frameThreads.push_back(m_frameThread);
			if (m_frameCount++ == 0 && m_isFirstFrameHeld)
			{
				m_frameStarted.release();
				m_releaseFrame.acquire();
			}
			--m_activeCount;
		}

	public:
		std::binary_semaphore m_frameStarted{0};
		std::binary_semaphore m_releaseFrame{0};
		std::atomic<bool> m_didOverlap = false;
		std::thread::id m_frameThread;
		std::thread::id m_completionThread;
		std::vector<std::thread::id> m_frameThreads;
		std::vector<std::thread::id> m_completionThreads;
		std::vector<int> m_completionFrameCounts;
		std::vector<int> m_completed;
		int m_frameCount = 0;

	private:
		const bool m_isFirstFrameHeld;
		std::atomic<int> m_activeCount = 0;
	};

	// 접수 여부만 검증할 때는 상태를 변경하지 않는다.
	void NoOpCompletion()
	{
	}

	// 종료가 새 완료를 거절할 때까지 첫 프레임을 붙잡아 큐를 남긴다.
	void ReleaseFrameAfterStop(CContentsRuntime &runtime, CTestContents &contents, bool &outDidClose)
	{
		const auto deadline = std::chrono::steady_clock::now() + kWaitTimeout;
		do
		{
			outDidClose = !runtime.PostCompletion(contents.GetInstanceId(), NoOpCompletion);
			if (outDidClose)
				break;
			std::this_thread::sleep_for(1ms);
		} while (std::chrono::steady_clock::now() < deadline);
		contents.m_releaseFrame.release();
	}

	// 같은 워커에서 밀린 프레임과 다른 콘텐츠의 완료가 번갈아 실행되는지 확인한다.
	int RunFrameCatchUpTest()
	{
		constexpr auto kFrameInterval = 100ms;
		constexpr std::size_t kSampleCount = 4;
		std::binary_semaphore samplesReady{0};
		std::vector<int> samples;
		CContentsRuntime runtime(1, kFrameInterval);
		auto upFramed = std::make_unique<CTestContents>(21, true);
		auto &framed = *upFramed;
		if (!runtime.RegisterContents(std::move(upFramed), true)
			|| !runtime.RegisterContents(std::make_unique<CTestContents>(22, false), false)
			|| !runtime.Start())
			return 20;

		const bool frameStarted = framed.m_frameStarted.try_acquire_for(kWaitTimeout);
		if (frameStarted)
			std::this_thread::sleep_for(kFrameInterval * kSampleCount);

		bool submitted = true;
		for (std::size_t i = 0; i < kSampleCount && submitted; ++i)
		{
			submitted = runtime.PostCompletion(22, [&framed, &samples, &samplesReady]
				{
					samples.push_back(framed.m_frameCount);
					if (samples.size() == kSampleCount)
						samplesReady.release();
				});
		}
		framed.m_releaseFrame.release();
		const bool finished = submitted && samplesReady.try_acquire_for(kWaitTimeout);
		runtime.Stop();
		if (!frameStarted || !finished)
			return 21;
		return samples == std::vector<int>{1, 2, 3, 4} ? 0 : 22;
	}

	// 실제 워커에서 프레임과 완료 큐, 배정과 종료를 검증한다.
	int RunPhaseOneTests()
	{
		CContentsRuntime noWorkers(0, 10ms);
		CContentsRuntime noInterval(1, 0ms);
		if (noWorkers.Start() || noInterval.Start())
			return 10;

		std::binary_semaphore otherWorkerRan{0};
		CContentsRuntime runtime(2, 10ms);
		auto upFirst = std::make_unique<CTestContents>(11, true);
		auto upSecond = std::make_unique<CTestContents>(12, false);
		auto upFrameless = std::make_unique<CTestContents>(13, false);
		auto &first = *upFirst;
		auto &second = *upSecond;
		auto &frameless = *upFrameless;

		if (runtime.PostCompletion(11, NoOpCompletion) || !runtime.RegisterContents(std::move(upFirst), true, 0)
			|| runtime.RegisterContents(std::make_unique<CTestContents>(11, false), false)
			|| runtime.RegisterContents(std::make_unique<CTestContents>(14, false), false, 2)
			|| !runtime.RegisterContents(std::move(upSecond), false)
			|| !runtime.RegisterContents(std::move(upFrameless), false, 0))
			return 11;

		if (!runtime.Start())
			return 12;

		const bool frameStarted = first.m_frameStarted.try_acquire_for(kWaitTimeout);
		const bool rejectedInvalidCompletion = !runtime.PostCompletion(99, NoOpCompletion) && !runtime.PostCompletion(11, {});
		const bool rejectedLateRegistration = !runtime.RegisterContents(std::make_unique<CTestContents>(15, false), false);
		const bool submitted = runtime.PostCompletion(11, [&first]
			{
				first.RecordCompletion(1);
			})
			&& runtime.PostCompletion(11, [&first]
			{
				first.RecordCompletion(2);
			})
			&& runtime.PostCompletion(13, [&frameless]
			{
				frameless.RecordCompletion(3);
			})
			&& runtime.PostCompletion(12, [&second, &otherWorkerRan]
				{
					second.RecordCompletion(4);
					otherWorkerRan.release();
				});
		const bool otherWorkerProgress = submitted && otherWorkerRan.try_acquire_for(kWaitTimeout);
		bool didClose = false;
		std::thread releaseThread(ReleaseFrameAfterStop, std::ref(runtime), std::ref(first), std::ref(didClose));
		runtime.Stop();
		releaseThread.join();

		if (!frameStarted || !otherWorkerProgress || !rejectedInvalidCompletion || !rejectedLateRegistration || !didClose)
			return 13;
		if (first.m_didOverlap || first.m_frameThread != first.m_completionThread
			|| first.m_completionThread != frameless.m_completionThread
			|| first.m_completionThread == second.m_completionThread)
			return 14;
		if (first.m_completed != std::vector<int>{1, 2} || second.m_completed != std::vector<int>{4}
			|| frameless.m_completed != std::vector<int>{3} || frameless.m_frameCount != 0
			|| second.m_frameCount != 0 || runtime.PostCompletion(11, NoOpCompletion) || runtime.Start())
			return 15;
		return 0;
	}

	// 첫 프레임 경계에서 빈 워커로 인계하고, 밀린 프레임과 완료 순서를 확인한다.
	int RunFrameDelegateTest()
	{
		constexpr auto kFrameInterval = 100ms;
		std::counting_semaphore<> queuedRan{0};
		CContentsRuntime runtime(2, kFrameInterval);
		auto upContents = std::make_unique<CTestContents>(31, true);
		auto &contents = *upContents;
		if (runtime.RequestDelegate(31, 1) || !runtime.RegisterContents(std::move(upContents), true, 0)
			|| runtime.RequestDelegate(31, 1) || !runtime.Start())
			return 30;

		const bool frameStarted = contents.m_frameStarted.try_acquire_for(kWaitTimeout);
		const bool rejectedInvalid = !runtime.RequestDelegate(99, 1) && !runtime.RequestDelegate(31, 2)
			&& !runtime.RequestDelegate(31, 0);
		const bool postedBeforeRequest = runtime.PostCompletion(31, [&contents]
			{
				contents.RecordCompletion(1);
			});
		const bool requested = runtime.RequestDelegate(31, 1);
		const bool rejectedPending = !runtime.RequestDelegate(31, 1);
		const bool postedWhilePending = runtime.PostCompletion(31, [&contents, &queuedRan]
			{
				contents.RecordCompletion(2);
				queuedRan.release();
			});
		if (frameStarted)
			std::this_thread::sleep_for(kFrameInterval * 2);
		contents.m_releaseFrame.release();

		const bool completed = postedWhilePending && queuedRan.try_acquire_for(kWaitTimeout);
		const bool rejectedCurrentWorker = completed && !runtime.RequestDelegate(31, 1);
		const bool postedAfterTransfer = completed && runtime.PostCompletion(31, [&contents]
			{
				contents.RecordCompletion(3);
			});
		runtime.Stop();
		if (!frameStarted || !rejectedInvalid || !postedBeforeRequest || !requested || !rejectedPending)
			return 31;
		if (!completed || !rejectedCurrentWorker || !postedAfterTransfer || runtime.RequestDelegate(31, 0))
			return 32;
		if (contents.m_didOverlap || contents.m_completed != std::vector<int>{1, 2, 3}
			|| contents.m_completionFrameCounts[0] != 2 || contents.m_completionFrameCounts[1] != 3
			|| contents.m_frameThreads.size() < 3
			|| contents.m_frameThreads.front() == contents.m_completionThread)
			return 33;
		for (std::size_t i = 1; i < contents.m_frameThreads.size(); ++i)
		{
			if (contents.m_frameThreads[i] != contents.m_completionThread)
				return 34;
		}
		for (const auto thread : contents.m_completionThreads)
		{
			if (thread != contents.m_completionThread)
				return 35;
		}
		return 0;
	}

	// 프레임 없는 콘텐츠도 실행 중인 완료가 끝난 뒤에만 인계한다.
	int RunCompletionDelegateTest()
	{
		std::binary_semaphore callbackStarted{0};
		std::binary_semaphore releaseCallback{0};
		std::counting_semaphore<> witnessRan{0};
		std::counting_semaphore<> transferredRan{0};
		std::atomic<bool> didCallbackFinish = false;
		std::atomic<bool> didStartEarly = false;
		CContentsRuntime runtime(2, 1h);
		auto upContents = std::make_unique<CTestContents>(41, false);
		auto &contents = *upContents;
		auto upWitness = std::make_unique<CTestContents>(42, false);
		auto &witness = *upWitness;
		if (!runtime.RegisterContents(std::move(upContents), false, 0)
			|| !runtime.RegisterContents(std::move(upWitness), false, 1) || !runtime.Start())
			return 40;

		const bool posted = runtime.PostCompletion(41, [&contents, &callbackStarted, &releaseCallback, &didCallbackFinish]
			{
				contents.RecordCompletion(1);
				callbackStarted.release();
				releaseCallback.acquire();
				didCallbackFinish = true;
			});
		const bool started = posted && callbackStarted.try_acquire_for(kWaitTimeout);
		const bool requested = started && runtime.RequestDelegate(41, 1);
		const bool queued = requested && runtime.PostCompletion(41, [&contents, &didCallbackFinish, &didStartEarly, &transferredRan]
			{
				didStartEarly = !didCallbackFinish;
				contents.RecordCompletion(2);
				transferredRan.release();
			});
		const bool witnessed = queued && runtime.PostCompletion(42, [&witness]
			{
				witness.RecordCompletion(1);
			}) && runtime.PostCompletion(42, [&witness, &witnessRan]
			{
				witness.RecordCompletion(2);
				witnessRan.release();
			});
		// 목적지가 두 번째 순회에 도달할 때까지 출발 콜백을 놓지 않는다.
		const bool destinationProgress = witnessed && witnessRan.try_acquire_for(kWaitTimeout);
		releaseCallback.release();
		const bool transferred = destinationProgress && transferredRan.try_acquire_for(kWaitTimeout);
		const bool requestedReturn = transferred && runtime.RequestDelegate(41, 0);
		const bool queuedReturn = requestedReturn && runtime.PostCompletion(41, [&contents, &transferredRan]
			{
				contents.RecordCompletion(3);
				transferredRan.release();
			});
		const bool returned = queuedReturn && transferredRan.try_acquire_for(kWaitTimeout);
		runtime.Stop();
		if (!returned || didStartEarly || contents.m_didOverlap || contents.m_frameCount != 0
			|| contents.m_completed != std::vector<int>{1, 2, 3})
			return 41;
		return contents.m_completionThreads[0] != witness.m_completionThread
			&& contents.m_completionThreads[1] == witness.m_completionThread
			&& contents.m_completionThreads[2] == contents.m_completionThreads[0] ? 0 : 42;
	}

	// 목적지가 바쁜 동안 두 인계를 쌓고 편입 전 재인계를 거절하는지 확인한다.
	int RunDelegateQueueTest()
	{
		std::binary_semaphore destinationStarted{0};
		std::binary_semaphore releaseDestination{0};
		std::binary_semaphore sourceScanned{0};
		std::counting_semaphore<> completionsRan{0};
		CContentsRuntime runtime(3, 1h);
		auto upFirst = std::make_unique<CTestContents>(61, false);
		auto &first = *upFirst;
		auto upSecond = std::make_unique<CTestContents>(62, false);
		auto &second = *upSecond;
		auto upDestination = std::make_unique<CTestContents>(64, false);
		auto &destination = *upDestination;
		if (!runtime.RegisterContents(std::move(upFirst), false, 0)
			|| !runtime.RegisterContents(std::move(upSecond), false, 0)
			|| !runtime.RegisterContents(std::make_unique<CTestContents>(63, false), false, 0)
			|| !runtime.RegisterContents(std::move(upDestination), false, 1) || !runtime.Start())
			return 60;

		const bool posted = runtime.PostCompletion(64, [&destination, &destinationStarted, &releaseDestination]
			{
				destination.RecordCompletion(1);
				destinationStarted.release();
				releaseDestination.acquire();
			});
		const bool started = posted && destinationStarted.try_acquire_for(kWaitTimeout);
		const bool requested = started && runtime.RequestDelegate(61, 1) && runtime.RequestDelegate(62, 1);
		const bool witnessed = requested && runtime.PostCompletion(63, [&sourceScanned]
			{
				sourceScanned.release();
			}) && sourceScanned.try_acquire_for(kWaitTimeout);
		const bool rejected = witnessed && !runtime.RequestDelegate(61, 2) && !runtime.RequestDelegate(62, 2);
		const bool queued = witnessed && runtime.PostCompletion(61, [&first, &completionsRan]
			{
				first.RecordCompletion(1);
				completionsRan.release();
			}) && runtime.PostCompletion(62, [&second, &completionsRan]
			{
				second.RecordCompletion(1);
				completionsRan.release();
			});
		releaseDestination.release();
		const bool completed = queued && completionsRan.try_acquire_for(kWaitTimeout) && completionsRan.try_acquire_for(kWaitTimeout);
		runtime.Stop();
		if (!rejected)
			return 61;
		return completed && first.m_completed == std::vector<int>{1} && second.m_completed == std::vector<int>{1}
			&& first.m_completionThread == destination.m_completionThread
			&& second.m_completionThread == destination.m_completionThread ? 0 : 62;
	}

	// 바쁜 워커에 여러 생산자가 쌓은 완료와 다음 유휴 깨움을 확인한다.
	int RunCompletionWakeTest()
	{
		std::binary_semaphore callbackStarted{0};
		std::binary_semaphore releaseCallback{0};
		std::binary_semaphore burstRan{0};
		std::binary_semaphore tailRan{0};
		constexpr int kProducerCount = 4;
		constexpr int kPostsPerProducer = 32;
		std::atomic<bool> didReject = false;
		int completed = 0;
		CContentsRuntime runtime(1, 1h);
		if (!runtime.RegisterContents(std::make_unique<CTestContents>(71, false), false, 0) || !runtime.Start())
			return 70;

		const bool posted = runtime.PostCompletion(71, [&callbackStarted, &releaseCallback]
			{
				callbackStarted.release();
				releaseCallback.acquire();
			});
		const bool started = posted && callbackStarted.try_acquire_for(kWaitTimeout);
		std::vector<std::thread> producers;
		for (int i = 0; started && i < kProducerCount; ++i)
		{
			producers.emplace_back([&runtime, &didReject, &completed, &burstRan]
				{
					for (int j = 0; j < kPostsPerProducer; ++j)
					{
						if (!runtime.PostCompletion(71, [&completed, &burstRan]
							{
								if (++completed == kProducerCount * kPostsPerProducer)
									burstRan.release();
							}))
							didReject = true;
					}
				});
		}
		for (auto &producer : producers)
			producer.join();
		releaseCallback.release();
		const bool drained = started && !didReject && burstRan.try_acquire_for(kWaitTimeout);
		const bool wokeAgain = drained && runtime.PostCompletion(71, [&tailRan]
			{
				tailRan.release();
			}) && tailRan.try_acquire_for(kWaitTimeout);
		runtime.Stop();
		return wokeAgain && completed == kProducerCount * kPostsPerProducer ? 0 : 71;
	}

	// 인계 확정 전에 종료하면 접수된 완료를 출발 워커에서 비운다.
	int RunDelegateStopTest()
	{
		CContentsRuntime runtime(2, 10ms);
		auto upContents = std::make_unique<CTestContents>(51, true);
		auto &contents = *upContents;
		if (!runtime.RegisterContents(std::move(upContents), true, 0) || !runtime.Start())
			return 50;

		const bool started = contents.m_frameStarted.try_acquire_for(kWaitTimeout);
		const bool requested = started && runtime.RequestDelegate(51, 1);
		const bool queued = requested && runtime.PostCompletion(51, [&contents]
			{
				contents.RecordCompletion(1);
			});
		bool didClose = false;
		std::thread releaseThread(ReleaseFrameAfterStop, std::ref(runtime), std::ref(contents), std::ref(didClose));
		runtime.Stop();
		releaseThread.join();
		return queued && didClose && !runtime.RequestDelegate(51, 1) && !contents.m_didOverlap && contents.m_frameCount == 1
			&& contents.m_completed == std::vector<int>{1} && contents.m_completionThread == contents.m_frameThread ? 0 : 51;
	}
}

// 네트워크 없이 테스트 월드의 런타임을 검증한다.
int main(void)
{
	const int phaseOneResult = WorldServer::RunPhaseOneTests();
	if (phaseOneResult != 0)
		return phaseOneResult;
	const int catchUpResult = WorldServer::RunFrameCatchUpTest();
	if (catchUpResult != 0)
		return catchUpResult;
	const int frameDelegateResult = WorldServer::RunFrameDelegateTest();
	if (frameDelegateResult != 0)
		return frameDelegateResult;
	const int completionDelegateResult = WorldServer::RunCompletionDelegateTest();
	if (completionDelegateResult != 0)
		return completionDelegateResult;
	const int delegateQueueResult = WorldServer::RunDelegateQueueTest();
	if (delegateQueueResult != 0)
		return delegateQueueResult;
	const int wakeResult = WorldServer::RunCompletionWakeTest();
	return wakeResult != 0 ? wakeResult : WorldServer::RunDelegateStopTest();
}