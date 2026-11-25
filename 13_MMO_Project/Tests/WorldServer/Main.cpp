#include "pch.h"
#include "IContents.h"
#include "ContentsHandle.h"
#include "CContentsRuntime.h"
#include "CContentsScheduler.h"
#include "ContentsDelegatePolicies.h"

namespace WorldServer
{
	using namespace std::chrono_literals;
	using ContentsRuntimeLib::CContentsRuntime;
	using ContentsRuntimeLib::CContentsScheduler;
	using ContentsRuntimeLib::SDelegateCandidate;
	using ContentsRuntimeLib::ContentsHandle;
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

	// 같은 ID를 다시 등록해도 이전 런타임의 Handle에는 완료를 접수하지 않는다.
	int RunCompletionHandleTest()
	{
		ContentsHandle staleHandle;
		int completed = 0;
		CContentsRuntime replacement(1, 1h);
		if (!replacement.RegisterContents(std::make_unique<CTestContents>(81, false), false))
			return 80;
		const auto replacementHandle = replacement.GetContentsHandle(81);
		if (!replacement.Start())
			return 81;
		{
			CContentsRuntime runtime(1, 1h);
			const auto unknownHandle = runtime.GetContentsHandle(81);
			if (!runtime.RegisterContents(std::make_unique<CTestContents>(81, false), false))
				return 82;
			staleHandle = runtime.GetContentsHandle(81);
			const auto recordCompletion = [&completed]
			{
				++completed;
			};
			if (runtime.PostCompletion(staleHandle, recordCompletion) || !runtime.Start())
				return 83;
			if (runtime.PostCompletion({}, recordCompletion) || runtime.PostCompletion(unknownHandle, recordCompletion)
				|| runtime.PostCompletion(staleHandle, {}) || replacement.PostCompletion(staleHandle, recordCompletion)
				|| !runtime.PostCompletion(staleHandle, recordCompletion))
				return 84;
			runtime.Stop();
			if (runtime.PostCompletion(staleHandle, recordCompletion))
				return 85;
		}
		if (replacement.PostCompletion(staleHandle, NoOpCompletion)
			|| !replacement.PostCompletion(replacementHandle, NoOpCompletion))
			return 86;
		replacement.Stop();
		return completed == 1 ? 0 : 87;
	}

	// 종료가 새 완료를 거절할 때까지 첫 프레임을 붙잡아 큐를 남긴다.
	void ReleaseFrameAfterStop(CContentsRuntime &runtime, ContentsHandle handle, CTestContents &contents, bool &outDidClose,
		int &outAccepted, int &outExecuted)
	{
		const auto deadline = std::chrono::steady_clock::now() + kWaitTimeout;
		const auto recordCompletion = [&outExecuted]
		{
			++outExecuted;
		};
		do
		{
			outDidClose = !runtime.PostCompletion(handle, recordCompletion);
			if (outDidClose)
			{
				outDidClose = !runtime.PostCompletion(handle, recordCompletion);
				break;
			}
			++outAccepted;
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
		std::function<void()> recordSample;
		CContentsRuntime runtime(1, kFrameInterval);
		auto upFramed = std::make_unique<CTestContents>(21, true);
		auto &framed = *upFramed;
		if (!runtime.RegisterContents(std::move(upFramed), true)
			|| !runtime.RegisterContents(std::make_unique<CTestContents>(22, false), false))
			return 20;
		const auto sampleHandle = runtime.GetContentsHandle(22);
		if (!runtime.Start())
			return 20;

		const bool frameStarted = framed.m_frameStarted.try_acquire_for(kWaitTimeout);
		if (frameStarted)
			std::this_thread::sleep_for(kFrameInterval * kSampleCount);

		// 각 순회에서 표본을 하나씩 남기고 다음 순회에 다시 확인한다.
		recordSample = [&runtime, &sampleHandle, &framed, &samples, &samplesReady, &recordSample]
		{
			samples.push_back(framed.m_frameCount);
			if (samples.size() == kSampleCount)
				samplesReady.release();
			else
				(void)runtime.PostCompletion(sampleHandle, recordSample);
		};
		const bool submitted = runtime.PostCompletion(sampleHandle, recordSample);
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

		if (!runtime.RegisterContents(std::move(upFirst), true, 0)
			|| runtime.RegisterContents(std::make_unique<CTestContents>(11, false), false)
			|| runtime.RegisterContents(std::make_unique<CTestContents>(14, false), false, 2)
			|| !runtime.RegisterContents(std::move(upSecond), false)
			|| !runtime.RegisterContents(std::move(upFrameless), false, 0))
			return 11;

		const auto firstHandle = runtime.GetContentsHandle(11);
		const auto secondHandle = runtime.GetContentsHandle(12);
		const auto framelessHandle = runtime.GetContentsHandle(13);
		if (!runtime.Start())
			return 12;

		const bool frameStarted = first.m_frameStarted.try_acquire_for(kWaitTimeout);
		const bool rejectedInvalidCompletion = !runtime.PostCompletion({}, NoOpCompletion) && !runtime.PostCompletion(firstHandle, {});
		const bool rejectedLateRegistration = !runtime.RegisterContents(std::make_unique<CTestContents>(15, false), false);
		const bool submitted = runtime.PostCompletion(firstHandle, [&first]
			{
				first.RecordCompletion(1);
			})
			&& runtime.PostCompletion(firstHandle, [&first]
			{
				first.RecordCompletion(2);
			})
			&& runtime.PostCompletion(framelessHandle, [&frameless]
			{
				frameless.RecordCompletion(3);
			})
			&& runtime.PostCompletion(secondHandle, [&second, &otherWorkerRan]
				{
					second.RecordCompletion(4);
					otherWorkerRan.release();
				});
		const bool otherWorkerProgress = submitted && otherWorkerRan.try_acquire_for(kWaitTimeout);
		bool didClose = false;
		int acceptedDuringStop = 0;
		int executedDuringStop = 0;
		std::thread releaseThread(ReleaseFrameAfterStop, std::ref(runtime), firstHandle, std::ref(first), std::ref(didClose),
			std::ref(acceptedDuringStop), std::ref(executedDuringStop));
		runtime.Stop();
		releaseThread.join();

		if (!frameStarted || !otherWorkerProgress || !rejectedInvalidCompletion || !rejectedLateRegistration || !didClose
			|| executedDuringStop != acceptedDuringStop)
			return 13;
		if (first.m_didOverlap || first.m_frameThread != first.m_completionThread
			|| first.m_completionThread != frameless.m_completionThread
			|| first.m_completionThread == second.m_completionThread)
			return 14;
		if (first.m_completed != std::vector<int>{1, 2} || second.m_completed != std::vector<int>{4}
			|| frameless.m_completed != std::vector<int>{3} || frameless.m_frameCount != 0
			|| second.m_frameCount != 0 || runtime.PostCompletion(firstHandle, NoOpCompletion) || runtime.Start())
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
			|| runtime.RequestDelegate(31, 1))
			return 30;
		const auto handle = runtime.GetContentsHandle(31);
		if (!runtime.Start())
			return 30;

		const bool frameStarted = contents.m_frameStarted.try_acquire_for(kWaitTimeout);
		const bool rejectedInvalid = !runtime.RequestDelegate(99, 1) && !runtime.RequestDelegate(31, 2)
			&& !runtime.RequestDelegate(31, 0);
		const bool postedBeforeRequest = runtime.PostCompletion(handle, [&contents]
			{
				contents.RecordCompletion(1);
			});
		const bool requested = runtime.RequestDelegate(31, 1);
		const bool rejectedPending = !runtime.RequestDelegate(31, 1);
		const auto pendingTimings = runtime.TakeEntryTimings().entries;
		const bool postedWhilePending = runtime.PostCompletion(handle, [&contents, &queuedRan]
			{
				contents.RecordCompletion(2);
				queuedRan.release();
			});
		if (frameStarted)
			std::this_thread::sleep_for(kFrameInterval * 2);
		contents.m_releaseFrame.release();

		const bool completed = postedWhilePending && queuedRan.try_acquire_for(kWaitTimeout);
		const bool rejectedCurrentWorker = completed && !runtime.RequestDelegate(31, 1);
		const bool postedAfterTransfer = completed && runtime.PostCompletion(handle, [&contents, &queuedRan]
			{
				contents.RecordCompletion(3);
				queuedRan.release();
			});
		const bool continuedAfterTransfer = postedAfterTransfer && queuedRan.try_acquire_for(kWaitTimeout);
		runtime.Stop();
		if (!frameStarted || !rejectedInvalid || !postedBeforeRequest || !requested || !rejectedPending)
			return 31;
		if (!completed || !rejectedCurrentWorker || !continuedAfterTransfer || runtime.RequestDelegate(31, 0))
			return 32;
		if (contents.m_didOverlap || contents.m_completed != std::vector<int>{1, 2, 3}
			|| contents.m_completionFrameCounts[0] != 2 || contents.m_completionFrameCounts[1] != 2
			|| contents.m_completionFrameCounts[2] < 3
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
		const auto timings = runtime.TakeEntryTimings().entries;
		if (pendingTimings.size() != 1 || pendingTimings[0].instanceId != 31 || pendingTimings[0].workerIndex != 0
			|| !pendingTimings[0].isDelegating || !pendingTimings[0].didRequestDelegate
			|| timings.size() != 1 || timings[0].instanceId != 31 || timings[0].workerIndex != 1
			|| timings[0].visitCount < 3 || timings[0].totalTime < 2 * kFrameInterval
			|| timings[0].isDelegating || !timings[0].didRequestDelegate)
			return 36;
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
			|| !runtime.RegisterContents(std::move(upWitness), false, 1))
			return 40;
		const auto handle = runtime.GetContentsHandle(41);
		const auto witnessHandle = runtime.GetContentsHandle(42);
		if (!runtime.Start())
			return 40;

		const bool posted = runtime.PostCompletion(handle, [&contents, &callbackStarted, &releaseCallback, &didCallbackFinish]
			{
				contents.RecordCompletion(1);
				callbackStarted.release();
				releaseCallback.acquire();
				didCallbackFinish = true;
			});
		const bool started = posted && callbackStarted.try_acquire_for(kWaitTimeout);
		const bool requested = started && runtime.RequestDelegate(41, 1);
		const bool queued = requested && runtime.PostCompletion(handle, [&contents, &didCallbackFinish, &didStartEarly, &transferredRan]
			{
				didStartEarly = !didCallbackFinish;
				contents.RecordCompletion(2);
				transferredRan.release();
			});
		const bool witnessed = queued && runtime.PostCompletion(witnessHandle, [&runtime, &witnessHandle, &witness, &witnessRan]
			{
				witness.RecordCompletion(1);
				(void)runtime.PostCompletion(witnessHandle, [&witness, &witnessRan]
					{
						witness.RecordCompletion(2);
						witnessRan.release();
					});
			});
		// 첫 완료에서 두 번째를 게시해 같은 묶음에서 실행되지 않게 한다.
		const bool destinationProgress = witnessed && witnessRan.try_acquire_for(kWaitTimeout);
		releaseCallback.release();
		const bool transferred = destinationProgress && transferredRan.try_acquire_for(kWaitTimeout);
		const bool requestedReturn = transferred && runtime.RequestDelegate(41, 0);
		const bool queuedReturn = requestedReturn && runtime.PostCompletion(handle, [&contents, &transferredRan]
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
			|| !runtime.RegisterContents(std::move(upDestination), false, 1))
			return 60;
		const auto firstHandle = runtime.GetContentsHandle(61);
		const auto secondHandle = runtime.GetContentsHandle(62);
		const auto sourceHandle = runtime.GetContentsHandle(63);
		const auto destinationHandle = runtime.GetContentsHandle(64);
		if (!runtime.Start())
			return 60;

		const bool posted = runtime.PostCompletion(destinationHandle, [&destination, &destinationStarted, &releaseDestination]
			{
				destination.RecordCompletion(1);
				destinationStarted.release();
				releaseDestination.acquire();
			});
		const bool started = posted && destinationStarted.try_acquire_for(kWaitTimeout);
		const bool requested = started && runtime.RequestDelegate(61, 1) && runtime.RequestDelegate(62, 1);
		const bool witnessed = requested && runtime.PostCompletion(sourceHandle, [&sourceScanned]
			{
				sourceScanned.release();
			}) && sourceScanned.try_acquire_for(kWaitTimeout);
		const bool rejected = witnessed && !runtime.RequestDelegate(61, 2) && !runtime.RequestDelegate(62, 2);
		const bool queued = witnessed && runtime.PostCompletion(firstHandle, [&first, &completionsRan]
			{
				first.RecordCompletion(1);
				completionsRan.release();
			}) && runtime.PostCompletion(secondHandle, [&second, &completionsRan]
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
		if (!runtime.RegisterContents(std::make_unique<CTestContents>(71, false), false, 0))
			return 70;
		const auto handle = runtime.GetContentsHandle(71);
		if (!runtime.Start())
			return 70;

		const bool posted = runtime.PostCompletion(handle, [&callbackStarted, &releaseCallback]
			{
				callbackStarted.release();
				releaseCallback.acquire();
			});
		const bool started = posted && callbackStarted.try_acquire_for(kWaitTimeout);
		std::vector<std::thread> producers;
		for (int i = 0; started && i < kProducerCount; ++i)
		{
			producers.emplace_back([&runtime, handle, &didReject, &completed, &burstRan]
				{
					for (int j = 0; j < kPostsPerProducer; ++j)
					{
						if (!runtime.PostCompletion(handle, [&completed, &burstRan]
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
		const bool wokeAgain = drained && runtime.PostCompletion(handle, [&tailRan]
			{
				tailRan.release();
			}) && tailRan.try_acquire_for(kWaitTimeout);
		runtime.Stop();
		return wokeAgain && completed == kProducerCount * kPostsPerProducer ? 0 : 71;
	}

	// 첫 방문에 확인한 완료를 모두 FIFO로 실행하고 새 완료는 다음 방문으로 넘긴다.
	int RunCompletionBatchTest(int initialCount)
	{
		std::binary_semaphore gateStarted{0};
		std::binary_semaphore releaseGate{0};
		std::binary_semaphore tailRan{0};
		std::vector<int> order;
		bool lateAccepted = false;
		CContentsRuntime runtime(1, 1h);
		for (ContentsInstanceId id = 101; id <= 103; ++id)
		{
			if (!runtime.RegisterContents(std::make_unique<CTestContents>(id, false), false, 0))
				return 100;
		}
		const auto gate = runtime.GetContentsHandle(101);
		const auto batch = runtime.GetContentsHandle(102);
		const auto peer = runtime.GetContentsHandle(103);
		if (!runtime.Start())
			return 100;
		const bool gated = runtime.PostCompletion(gate, [&gateStarted, &releaseGate]
			{
				gateStarted.release();
				releaseGate.acquire();
			}) && gateStarted.try_acquire_for(kWaitTimeout);
		bool queued = gated && runtime.PostCompletion(batch, [&runtime, &batch, &order, &tailRan, &lateAccepted, initialCount]
			{
				order.push_back(1);
				lateAccepted = runtime.PostCompletion(batch, [&order, &tailRan, initialCount]
					{
						order.push_back(initialCount + 1);
						tailRan.release();
					});
			});
		for (int sequence = 2; sequence <= initialCount && queued; ++sequence)
		{
			queued = runtime.PostCompletion(batch, [&order, sequence]
				{
					order.push_back(sequence);
				});
		}
		queued = queued && runtime.PostCompletion(peer, [&order]
			{
				order.push_back(0);
			});
		releaseGate.release();
		const bool completed = queued && tailRan.try_acquire_for(kWaitTimeout);
		runtime.Stop();
		std::vector<int> expected;
		for (int sequence = 1; sequence <= initialCount + 1; ++sequence)
		{
			expected.push_back(sequence);
			if (sequence == initialCount)
				expected.push_back(0);
		}
		return completed && lateAccepted && order == expected ? 0 : 101;
	}

	// 뒤쪽 엔트리의 프레임 시각이 와도 이미 확인한 완료를 마친다.
	int RunCompletionBatchOverrunTest()
	{
		constexpr auto kFrameInterval = 100ms;
		std::binary_semaphore gateStarted{0};
		std::binary_semaphore releaseGate{0};
		std::binary_semaphore firstStarted{0};
		std::binary_semaphore releaseFirst{0};
		std::binary_semaphore frameVisitDone{0};
		int framesBefore = 0;
		int framesAfter = 0;
		CContentsRuntime runtime(1, kFrameInterval);
		auto upFramed = std::make_unique<CTestContents>(113, false);
		auto &framed = *upFramed;
		if (!runtime.RegisterContents(std::make_unique<CTestContents>(111, false), false, 0)
			|| !runtime.RegisterContents(std::make_unique<CTestContents>(112, false), false, 0)
			|| !runtime.RegisterContents(std::move(upFramed), true, 0))
			return 110;
		const auto gate = runtime.GetContentsHandle(111);
		const auto batch = runtime.GetContentsHandle(112);
		const auto frameHandle = runtime.GetContentsHandle(113);
		if (!runtime.Start())
			return 110;
		const bool gated = runtime.PostCompletion(gate, [&gateStarted, &releaseGate]
			{
				gateStarted.release();
				releaseGate.acquire();
			}) && gateStarted.try_acquire_for(kWaitTimeout);
		const bool queued = gated && runtime.PostCompletion(batch, [&framed, &framesBefore, &firstStarted, &releaseFirst]
			{
				framesBefore = framed.m_frameCount;
				firstStarted.release();
				releaseFirst.acquire();
			}) && runtime.PostCompletion(batch, [&framed, &framesAfter]
			{
				framesAfter = framed.m_frameCount;
			});
		releaseGate.release();
		const bool started = queued && firstStarted.try_acquire_for(kWaitTimeout);
		if (started)
			std::this_thread::sleep_for(kFrameInterval * 2);
		const bool witnessQueued = started && runtime.PostCompletion(frameHandle, [&frameVisitDone]
			{
				frameVisitDone.release();
			});
		releaseFirst.release();
		const bool completed = witnessQueued && frameVisitDone.try_acquire_for(kWaitTimeout);
		runtime.Stop();
		return completed && framesAfter == framesBefore && framed.m_frameCount > framesAfter ? 0 : 111;
	}

	// 같은 워커에서 실행한 콘텐츠도 방문 시간을 각자 기록한다.
	int RunEntryTimingTest()
	{
		using Clock = std::chrono::steady_clock;
		std::binary_semaphore completed{0};
		CContentsRuntime runtime(2, 100ms);
		auto upFramed = std::make_unique<CTestContents>(121, true);
		auto &framed = *upFramed;
		if (!runtime.RegisterContents(std::move(upFramed), true, 0)
			|| !runtime.RegisterContents(std::make_unique<CTestContents>(122, false), false, 0))
			return 120;
		const auto handle = runtime.GetContentsHandle(122);
		if (!runtime.Start())
			return 120;
		const bool started = framed.m_frameStarted.try_acquire_for(kWaitTimeout);
		const auto baseline = runtime.TakeEntryTimings();
		const auto duringFrame = runtime.TakeEntryTimings().entries;
		const auto start = Clock::now();
		const bool posted = started && runtime.PostCompletion(handle, [&completed]
			{
				std::this_thread::sleep_for(10ms);
				completed.release();
			});
		if (posted)
			std::this_thread::sleep_for(20ms);
		const auto frameHoldTime = Clock::now() - start;
		framed.m_releaseFrame.release();
		const bool ran = posted && completed.try_acquire_for(kWaitTimeout);
		runtime.Stop();
		const auto timings = runtime.TakeEntryTimings().entries;
		const auto cleared = runtime.TakeEntryTimings().entries;
		if (!ran || baseline.workerCount != 2 || baseline.entries.size() != 2
			|| duringFrame.size() != 2 || timings.size() != 2 || cleared.size() != 2)
			return 121;
		if (duringFrame[0].visitCount != 0 || duringFrame[1].visitCount != 0
			|| timings[0].instanceId != 121 || timings[0].workerIndex != 0 || timings[0].totalTime < frameHoldTime
			|| timings[1].instanceId != 122 || timings[1].workerIndex != 0 || timings[1].totalTime < 10ms
			|| timings[0].visitCount == 0 || timings[1].visitCount == 0)
			return 122;
		for (const auto &timing : cleared)
		{
			if (timing.totalTime != Clock::duration::zero() || timing.visitCount != 0)
				return 123;
		}
		return 0;
	}

	struct SSchedulerTestPolicy
	{
		// 이동 없음, 자기 자신, 정상 인계 순으로 후보를 반환한다.
		std::optional<SDelegateCandidate> operator()(const ContentsRuntimeLib::SEntryTimings &timings,
			const std::optional<ContentsRuntimeLib::SDelegateSubmitResult> &lastSubmit)
		{
			const auto now = std::chrono::steady_clock::now();
			for (const auto &timing : timings.entries)
			{
				if (timing.instanceId == 93 && timing.workerIndex == 2 && timing.visitCount != 0)
					didReadTiming = true;
			}
			if (evaluationCount != 0 && now - lastEvaluation < 20ms)
				didEvaluateEarly = true;
			lastEvaluation = now;
			if (++evaluationCount == 2)
				return SDelegateCandidate{93, 1};
			using ContentsRuntimeLib::EDelegateSubmitStatus;
			if (evaluationCount == 3)
			{
				didReadSubmitResults = lastSubmit && lastSubmit->candidate.instanceId == 93
					&& lastSubmit->status == EDelegateSubmitStatus::NotSubmitted;
				return SDelegateCandidate{91, timings.workerCount};
			}
			if (evaluationCount == 4)
			{
				didReadSubmitResults = didReadSubmitResults && lastSubmit && lastSubmit->candidate.instanceId == 91
					&& lastSubmit->candidate.destinationWorkerIndex == timings.workerCount
					&& lastSubmit->status == EDelegateSubmitStatus::Rejected;
				return SDelegateCandidate{91, 1};
			}
			if (evaluationCount == 5)
				didReadSubmitResults = didReadSubmitResults && lastSubmit && lastSubmit->candidate.instanceId == 91
					&& lastSubmit->candidate.destinationWorkerIndex == 1 && lastSubmit->status == EDelegateSubmitStatus::Accepted;
			if (evaluationCount == 6)
			{
				didReadSubmitResults = didReadSubmitResults && !lastSubmit;
				didSubmit.release();
			}
			return std::nullopt;
		}

		std::binary_semaphore didSubmit{0};
		std::chrono::steady_clock::time_point lastEvaluation{};
		int evaluationCount = 0;
		bool didReadSubmitResults = false;
		bool didEvaluateEarly = false;
		bool didReadTiming = false;
	};

	// 같은 스케줄러 생성자에 정책을 주입해 기존 Delegate 경로를 확인한다.
	template <typename TPolicy>
	int RunSchedulerTest(TPolicy &policy, bool useEmptyDestination = false)
	{
		std::counting_semaphore<> completed{0};
		std::thread::id schedulerThread;
		CContentsRuntime runtime(3, 1ms);
		auto upSource = std::make_unique<CTestContents>(91, false);
		auto &source = *upSource;
		auto upDestination = std::make_unique<CTestContents>(92, false);
		auto &destination = *upDestination;
		if (!runtime.RegisterContents(std::move(upSource), false, 0)
			|| (useEmptyDestination && !runtime.RegisterContents(std::make_unique<CTestContents>(90, false), false, 0))
			|| (!useEmptyDestination && !runtime.RegisterContents(std::move(upDestination), false, 1))
			|| !runtime.RegisterContents(std::make_unique<CContentsScheduler>(93, runtime, 20ms, std::ref(policy)), true, 2))
			return 90;

		const auto sourceHandle = runtime.GetContentsHandle(91);
		const auto destinationHandle = runtime.GetContentsHandle(92);
		const auto schedulerHandle = runtime.GetContentsHandle(93);
		if (!runtime.Start())
			return 91;

		const bool submitted = policy.didSubmit.try_acquire_for(kWaitTimeout);
		const bool posted = submitted && runtime.PostCompletion(sourceHandle, [&source, &completed]
			{
				source.RecordCompletion(1);
				completed.release();
			}) && (useEmptyDestination || runtime.PostCompletion(destinationHandle, [&destination, &completed]
			{
				destination.RecordCompletion(1);
				completed.release();
			})) && runtime.PostCompletion(schedulerHandle, [&schedulerThread, &completed]
			{
				schedulerThread = std::this_thread::get_id();
				completed.release();
			});
		const bool ran = posted && completed.try_acquire_for(kWaitTimeout) && completed.try_acquire_for(kWaitTimeout)
			&& (useEmptyDestination || completed.try_acquire_for(kWaitTimeout));
		runtime.Stop();
		return ran && policy.didReadTiming && !policy.didEvaluateEarly && source.m_completionThread != schedulerThread
			&& (useEmptyDestination || source.m_completionThread == destination.m_completionThread) ? 0 : 92;
	}

	struct SDelegatePolicyFixture
	{
		// 워커 부하와 평가 간격을 고정하여 정책만 시험한다.
		std::optional<SDelegateCandidate> Evaluate(ContentsRuntimeLib::DelegatePolicies::CPersistentOverloadDelegatePolicy &policy,
			std::chrono::milliseconds sourceTime, bool didRequestDelegate = false)
		{
			now += 100ms;
			return policy.EvaluateAt({3, {{90, 0, sourceTime - 20ms, 1, false, false}, {91, 0, 20ms, 1, false, didRequestDelegate},
				{92, 1, 30ms, 1, false, false}, {93, 2, 10ms, 1, false, false}}}, now);
		}

		std::chrono::steady_clock::time_point now{};
	};

	// 과부하의 지속과 회복을 구분하고 근거 없는 인계는 하지 않는다.
	int RunPersistentPolicyTest()
	{
		using namespace ContentsRuntimeLib::DelegatePolicies;
		SDelegatePolicyFixture fixture;
		CPersistentOverloadDelegatePolicy policy({200ms, 2}, {90, 91});
		for (const auto sourceTime : {90ms, 90ms, 90ms, 60ms, 60ms, 90ms, 90ms, 90ms})
		{
			if (fixture.Evaluate(policy, sourceTime))
				return 130;
		}
		const auto selected = fixture.Evaluate(policy, 90ms);
		if (!selected || selected->instanceId != 91 || selected->destinationWorkerIndex != 2)
			return 131;
		const ContentsRuntimeLib::SEntryTimings pending{3, {{90, 0, 70ms, 1, false, false}, {91, 0, 20ms, 1, true, true},
			{92, 1, 30ms, 1, false, false}, {93, 2, 10ms, 1, false, false}}};
		const ContentsRuntimeLib::SDelegateSubmitResult accepted{*selected, ContentsRuntimeLib::EDelegateSubmitStatus::Accepted};
		if (policy.EvaluateAt(pending, fixture.now += 100ms, accepted))
			return 132;
		for (int i = 0; i < 5; ++i)
		{
			if (policy.EvaluateAt(pending, fixture.now += 100ms))
				return 132;
		}

		CPersistentOverloadDelegatePolicy noFacts({200ms, 2});
		const CContentsScheduler::SelectCandidate directPolicy = std::ref(noFacts);
		if (directPolicy({2, {{91, 0, 90ms, 1, false, false}, {92, 1, 30ms, 1, false, false}}}, std::nullopt))
			return 133;
		CPersistentOverloadDelegatePolicy unknownDestination({200ms, 2}, {90, 91});
		for (int i = 0; i < 6; ++i)
		{
			fixture.now += 100ms;
			const std::vector<ContentsRuntimeLib::SEntryTimingSnapshot> timings{
				{90, 0, 70ms, 1, false, false}, {91, 0, 20ms, 1, false, false}, {92, 1, {}, 0, false, false}};
			if (noFacts.EvaluateAt({2, {{91, 0, 90ms, 1, false, false}, {92, 1, 30ms, 1, false, false}}}, fixture.now)
				|| unknownDestination.EvaluateAt({2, timings}, fixture.now))
				return 133;
		}
		CPersistentOverloadDelegatePolicy mixedAssignment({200ms, 2}, {90, 91});
		for (int i = 0; i < 4; ++i)
		{
			if (fixture.Evaluate(mixedAssignment, 90ms))
				return 134;
		}
		if (fixture.Evaluate(mixedAssignment, 90ms, true))
			return 134;
		for (int i = 0; i < 3; ++i)
		{
			if (fixture.Evaluate(mixedAssignment, 90ms))
				return 134;
		}
		if (!fixture.Evaluate(mixedAssignment, 90ms))
			return 134;
		return 0;
	}

	// 빈 워커만 부하 0으로 보고 미계측과 인계 대기를 제외한다.
	int RunEmptyWorkerPolicyTest()
	{
		using namespace ContentsRuntimeLib;
		using namespace DelegatePolicies;
		CPersistentOverloadDelegatePolicy empty({200ms, 2}, {91});
		CPersistentOverloadDelegatePolicy unmeasured({200ms, 2}, {91});
		CPersistentOverloadDelegatePolicy inbound({200ms, 2}, {91});
		SEntryTimings timings{2, {{90, 0, 70ms, 1, false, false}, {91, 0, 20ms, 1, false, false}}};
		auto zeroVisits = timings;
		zeroVisits.entries.push_back({92, 1, {}, 0, false, false});
		auto pending = timings;
		pending.entries.push_back({92, 1, {}, 0, true, true});
		for (int i = 0; i < 5; ++i)
		{
			const auto now = std::chrono::steady_clock::time_point{} + i * 100ms;
			const auto candidate = empty.EvaluateAt(timings, now);
			if (unmeasured.EvaluateAt(zeroVisits, now) || inbound.EvaluateAt(i == 3 ? pending : timings, now))
				return 135;
			if (i < 4 ? candidate.has_value() : !candidate || candidate->instanceId != 91 || candidate->destinationWorkerIndex != 1)
				return 136;
		}
		return 0;
	}

	// 접수 거절 뒤에는 다른 쌍을 시도하고, 접수된 요청은 편입까지 한 건만 유지한다.
	int RunDelegateRetryPolicyTest()
	{
		using namespace ContentsRuntimeLib;
		using namespace DelegatePolicies;
		using Clock = std::chrono::steady_clock;
		const SEntryTimings before{3, {{90, 0, 60ms, 1, false, false}, {91, 0, 30ms, 1, false, false}}};
		CPersistentOverloadDelegatePolicy policy({200ms, 2}, {91});
		Clock::time_point now{};
		std::optional<SDelegateCandidate> candidate;
		for (int i = 0; i < 5; ++i, now += 100ms)
			candidate = policy.EvaluateAt(before, now);
		if (!candidate || candidate->instanceId != 91 || candidate->destinationWorkerIndex != 1)
			return 137;
		if (policy.EvaluateAt(before, now, SDelegateSubmitResult{*candidate, EDelegateSubmitStatus::Rejected}))
			return 138;
		for (int i = 0; i < 4; ++i)
			candidate = policy.EvaluateAt(before, now += 100ms);
		if (!candidate || candidate->instanceId != 91 || candidate->destinationWorkerIndex != 2)
			return 139;

		auto pending = before;
		pending.entries[1].isDelegating = true;
		pending.entries[1].didRequestDelegate = true;
		if (policy.EvaluateAt(pending, now += 100ms, SDelegateSubmitResult{*candidate, EDelegateSubmitStatus::Accepted}))
			return 140;
		for (int i = 0; i < 8; ++i)
		{
			if (policy.EvaluateAt(pending, now += 100ms))
				return 140;
		}
		pending.entries[1].workerIndex = 2;
		if (policy.EvaluateAt(pending, now += 100ms))
			return 140;
		pending.entries[1].isDelegating = false;
		if (policy.EvaluateAt(pending, now += 100ms))
			return 141;

		// 편입 이후 이전 워커는 한가하고 목적지에서 역이동 이득이 나더라도 첫 관찰에는 막는다.
		SEntryTimings after{3, {{90, 0, 10ms, 1, false, false}, {91, 2, 30ms, 1, false, false},
			{92, 2, 60ms, 1, false, false}, {93, 1, 50ms, 1, false, false}}};
		for (int i = 0; i < 4; ++i)
		{
			if (policy.EvaluateAt(after, now += 100ms))
				return 142;
		}
		for (int i = 0; i < 4; ++i)
			candidate = policy.EvaluateAt(after, now += 100ms);
		if (!candidate || candidate->instanceId != 91 || candidate->destinationWorkerIndex != 0)
			return 143;
		return 0;
	}

	// 제출 결과가 없거나 편입 후 표본이 부족해도 판단을 다시 시작할 수 있다.
	int RunIncompleteDelegateFeedbackTest()
	{
		using namespace ContentsRuntimeLib;
		using namespace DelegatePolicies;
		const SEntryTimings before{3, {{90, 0, 45ms, 1, false, false}, {91, 0, 30ms, 1, false, false},
			{94, 0, 15ms, 1, false, false}}};
		CPersistentOverloadDelegatePolicy missingResult({200ms, 2}, {91, 94});
		CPersistentOverloadDelegatePolicy missingTiming({200ms, 2}, {91, 94});
		std::chrono::steady_clock::time_point now{};
		std::optional<SDelegateCandidate> first, second;
		for (int i = 0; i < 5; ++i, now += 100ms)
		{
			first = missingResult.EvaluateAt(before, now);
			second = missingTiming.EvaluateAt(before, now);
		}
		if (!first || !second || second->instanceId != 91 || second->destinationWorkerIndex != 1)
			return 145;
		SEntryTimings adopted{3, {{90, 0, 65ms, 1, false, false}, {91, 1, {}, 0, false, true}, {94, 0, 25ms, 1, false, false}}};
		if (missingResult.EvaluateAt(before, now)
			|| missingTiming.EvaluateAt(adopted, now, SDelegateSubmitResult{*second, EDelegateSubmitStatus::Accepted}))
			return 146;
		adopted.entries[1].didRequestDelegate = false;
		for (int i = 0; i < 4; ++i)
		{
			now += 100ms;
			first = missingResult.EvaluateAt(before, now);
			if (missingTiming.EvaluateAt(adopted, now))
				return 147;
		}
		if (!first)
			return 148;
		if (missingTiming.EvaluateAt(adopted, now += 100ms))
			return 149;
		second = missingTiming.EvaluateAt(adopted, now += 100ms);
		return second && second->instanceId == 94 && second->destinationWorkerIndex == 2 ? 0 : 150;
	}

	struct SPersistentSchedulerTestPolicy
	{
		// 실제 평가 호출에서 받은 스냅샷을 확인하고 부하 판단에는 시험값을 쓴다.
		std::optional<SDelegateCandidate> operator()(const ContentsRuntimeLib::SEntryTimings &timings,
			const std::optional<ContentsRuntimeLib::SDelegateSubmitResult> &lastSubmit)
		{
			for (const auto &timing : timings.entries)
			{
				if (timing.instanceId == 93 && timing.workerIndex == 2 && timing.visitCount != 0)
					didReadTiming = true;
				if (timing.instanceId == 91 && timing.workerIndex == 1 && !timing.isDelegating && !didSignal)
				{
					didSignal = true;
				}
			}
			auto input = timings;
			for (auto &timing : input.entries)
			{
				timing.totalTime = timing.instanceId == 91 ? 20ms : 70ms;
				timing.visitCount = 1;
			}
			const auto candidate = policy.EvaluateAt(input, fixture.now += 100ms, lastSubmit);
			if (didSignal && !didNotify)
			{
				didNotify = true;
				didSubmit.release();
			}
			return candidate;
		}

		SDelegatePolicyFixture fixture;
		ContentsRuntimeLib::DelegatePolicies::CPersistentOverloadDelegatePolicy policy{
			{200ms, 2}, {91}};
		std::binary_semaphore didSubmit{0};
		bool didReadTiming = false;
		bool didEvaluateEarly = false;
		bool didSignal = false;
		bool didNotify = false;
	};

	// 인계 확정 전에 종료하면 접수된 완료를 출발 워커에서 비운다.
	int RunDelegateStopTest()
	{
		CContentsRuntime runtime(2, 10ms);
		auto upContents = std::make_unique<CTestContents>(51, true);
		auto &contents = *upContents;
		if (!runtime.RegisterContents(std::move(upContents), true, 0))
			return 50;
		const auto handle = runtime.GetContentsHandle(51);
		if (!runtime.Start())
			return 50;

		const bool started = contents.m_frameStarted.try_acquire_for(kWaitTimeout);
		const bool requested = started && runtime.RequestDelegate(51, 1);
		const bool queued = requested && runtime.PostCompletion(handle, [&contents]
			{
				contents.RecordCompletion(1);
			});
		bool didClose = false;
		int acceptedDuringStop = 0;
		int executedDuringStop = 0;
		std::thread releaseThread(ReleaseFrameAfterStop, std::ref(runtime), handle, std::ref(contents), std::ref(didClose),
			std::ref(acceptedDuringStop), std::ref(executedDuringStop));
		runtime.Stop();
		releaseThread.join();
		return queued && didClose && executedDuringStop == acceptedDuringStop && !runtime.RequestDelegate(51, 1)
			&& !contents.m_didOverlap && contents.m_frameCount == 1
			&& contents.m_completed == std::vector<int>{1} && contents.m_completionThread == contents.m_frameThread ? 0 : 51;
	}
}

// 네트워크 없이 테스트 월드의 런타임을 검증한다.
int main(void)
{
	const int handleResult = WorldServer::RunCompletionHandleTest();
	if (handleResult != 0)
		return handleResult;
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
	if (wakeResult != 0)
		return wakeResult;
	const int stopResult = WorldServer::RunDelegateStopTest();
	if (stopResult != 0)
		return stopResult;
	const int batchResult = WorldServer::RunCompletionBatchTest(65);
	if (batchResult != 0)
		return batchResult;
	const int overrunResult = WorldServer::RunCompletionBatchOverrunTest();
	if (overrunResult != 0)
		return overrunResult;
	const int timingResult = WorldServer::RunEntryTimingTest();
	if (timingResult != 0)
		return timingResult;
	const int policyResult = WorldServer::RunPersistentPolicyTest();
	if (policyResult != 0)
		return policyResult;
	const int emptyWorkerResult = WorldServer::RunEmptyWorkerPolicyTest();
	if (emptyWorkerResult != 0)
		return emptyWorkerResult;
	const int retryPolicyResult = WorldServer::RunDelegateRetryPolicyTest();
	if (retryPolicyResult != 0)
		return retryPolicyResult;
	const int incompleteFeedbackResult = WorldServer::RunIncompleteDelegateFeedbackTest();
	if (incompleteFeedbackResult != 0)
		return incompleteFeedbackResult;
	WorldServer::SSchedulerTestPolicy fixed;
	const int fixedResult = WorldServer::RunSchedulerTest(fixed);
	if (fixedResult != 0)
		return fixedResult;
	if (!fixed.didReadSubmitResults)
		return 144;
	WorldServer::SPersistentSchedulerTestPolicy persistent;
	return WorldServer::RunSchedulerTest(persistent, true);
}