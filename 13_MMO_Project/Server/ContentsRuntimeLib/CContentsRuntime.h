#pragma once

namespace ContentsRuntimeLib
{
	class ContentsHandle;

	// TakeEntryTimings()가 반환하는 원본 계측값
	struct SEntryTimingSnapshot
	{
		ContentsInstanceId 					instanceId;
		std::size_t 						workerIndex;
		std::chrono::steady_clock::duration totalTime;
		std::size_t 						visitCount;
		bool 								isDelegating;
		// 인계 요청부터 편입 뒤 첫 수집까지 전환 구간으로 표시한다.
		bool didRequestDelegate;
	};

	// 전체 워커 수와 엔트리별 방문 표본을 같은 수집 결과에 담는다.
	struct SEntryTimings
	{
		std::size_t 						workerCount;
		std::vector<SEntryTimingSnapshot> 	entries;
	};

	class CContentsRuntime final
	{
	public:
		// 워커 수와 프레임 주기를 지정한다. 등록·시작·종료는 하나의 제어 스레드가 맡는다.
		CContentsRuntime(std::size_t workerCount, std::chrono::milliseconds frameInterval);
		~CContentsRuntime();

		// 시작 전에 인스턴스를 등록한다. 워커를 생략하면 등록 수가 가장 적은 곳에 배정한다.
		[[nodiscard]]
		bool RegisterContents(std::unique_ptr<IContents> upContents, bool usesFrame,
			std::optional<std::size_t> workerIndex = std::nullopt);

		// 등록된 인스턴스의 담당 워커를 시작한다.
		// 잘못된 설정은 false를 반환하고, 워커 생성 예외는 프로세스를 종료한다.
		[[nodiscard]]
		bool Start() noexcept;

		// 새 요청을 닫고 미확정 인계를 취소한다. 접수된 콜백을 처리한 뒤 워커를 종료한다.
		void Stop();

		// 외부 스레드에서 완료를 접수한다. true는 큐 삽입 성공이며 실행 완료가 아니다.
		// 프레임과 완료 콜백에서 빠져나온 예외는 복구하지 않는다.
		[[nodiscard]]
		bool 		PostCompletion(const ContentsHandle &handle, std::function<void()> completion);

		// true는 접수만 뜻한다. 담당 워커가 실행 경계에서 인계하며, Stop이 먼저면 취소한다.
		[[nodiscard]]
		bool 		RequestDelegate(ContentsInstanceId instanceId, std::size_t destinationWorkerIndex);

		// 시작 전에 등록된 인스턴스의 Handle을 얻는다. 없으면 빈 Handle을 반환한다.
		[[nodiscard]]
		ContentsHandle 	GetContentsHandle(ContentsInstanceId instanceId);

		// 엔트리별 완료 방문 누계와 배정을 가져오고 누계를 초기화한다.
		[[nodiscard]]
		SEntryTimings TakeEntryTimings();

	private:
		friend struct SCompletionMailbox;
		struct SWorker;
		struct SEntry;
		struct SRoute
		{
			SEntry *pEntry;
			SWorker *pWorker;
		};

		enum class EState
		{
			Registering,
			Running,
			Stopped
		};

		// 담당 인스턴스의 프레임과 완료를 순서대로 실행한다.
		void runWorker(SWorker &worker);

	private:
		const std::chrono::milliseconds 				m_frameInterval;
		// 런타임 상태와 배정 경로를 보호하며, 워커의 작업 락보다 먼저 잡는다.
		std::mutex 										m_registrationLock;
		std::unordered_map<ContentsInstanceId, SRoute> 	m_routes;
		EState 											m_state = EState::Registering;
		std::vector<std::unique_ptr<SWorker>> 			m_workers;
	};
}