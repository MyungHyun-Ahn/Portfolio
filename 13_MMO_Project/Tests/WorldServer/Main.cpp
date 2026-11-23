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
}

// 네트워크 없이 테스트 월드의 런타임을 검증한다.
int main(void)
{
	const int result = WorldServer::RunPhaseOneTests();
	return result != 0 ? result : WorldServer::RunFrameCatchUpTest();
}