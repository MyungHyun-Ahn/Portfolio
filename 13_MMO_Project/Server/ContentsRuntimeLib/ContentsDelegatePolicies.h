#pragma once

namespace ContentsRuntimeLib
{
	struct SDelegateCandidate;
	struct SDelegateSubmitResult;
	struct SEntryTimingSnapshot;
	struct SEntryTimings;

	// 여기에 스케줄링 정책을 펑터 형식으로 정의합니다.
	namespace DelegatePolicies
	{
		struct SPersistentOverloadSettings
		{
			// 관찰 시간
			std::chrono::milliseconds observationPeriod{10000};
			// 최소 관찰 구간 수
			std::size_t minimumWindows = 5;
		};

		// 과부하가 지속되는 워커를 관찰하고 다른 워커로 인계할 후보를 선택하는 펑터
		class CPersistentOverloadDelegatePolicy final
		{
		public:
			using Clock = std::chrono::steady_clock;

			// 자동 인계가 허용된 콘텐츠 ID를 지정한다.
			CPersistentOverloadDelegatePolicy(SPersistentOverloadSettings settings,
				std::vector<ContentsInstanceId> eligibleInstances = {});

			// 평가마다 이동 없음 또는 인계 후보 한 건을 반환한다.
			[[nodiscard]]
			std::optional<SDelegateCandidate> operator()(const SEntryTimings &timings,
				const std::optional<SDelegateSubmitResult> &lastSubmit);

			// 시각을 명시해 동일한 판단 경로를 시험한다.
			[[nodiscard]]
			std::optional<SDelegateCandidate> EvaluateAt(const SEntryTimings &timings, Clock::time_point now,
				const std::optional<SDelegateSubmitResult> &lastSubmit = std::nullopt);

		private:
			// 정책이 유지하는 워커별 관찰 상태
			struct SWorkerLoad
			{
				std::optional<double> latest;
				Clock::duration latestTime{};
				Clock::duration totalTime{};
				Clock::duration elapsed{};
				std::size_t highCount = 0;
				std::size_t observedCount = 0;
				std::size_t emptyCount = 0;
				bool hasEntry = false;
			};

			enum class EDelegateStage
			{
				Proposed,
				Accepted,
				Verifying
			};

			struct SDelegateMove
			{
				SDelegateCandidate candidate;
				std::size_t source;
				EDelegateStage stage;
			};

			struct SEntryLoad
			{
				Clock::duration totalTime{};
				std::size_t observedCount = 0;
			};

			// 접수 결과와 엔트리 배정으로 이동 상태를 갱신한다. 대기 또는 전환 구간은 부하 계산에 쓰지 않는다.
			bool canObserve(const SEntryTimings &snapshot, const std::optional<SDelegateSubmitResult> &lastSubmit);
			// 편입 뒤 새 표본으로 양쪽 부하를 확인하고 여전히 과부하인 워커를 다시 관찰한다.
			void verifyDelegate(Clock::duration elapsed);
			// 후보와 목적지의 조합 중 예상 최대 부하를 가장 많이 낮추는 조합을 선택한다.
			std::optional<SDelegateCandidate> selectCandidate(std::size_t source, const std::vector<SEntryTimingSnapshot> &timings) const;
			// 다음 관찰에 이전 관찰 표본이 남지 않게 비운다.
			void clearObservation();

		private:
			SPersistentOverloadSettings 						m_settings;
			std::vector<ContentsInstanceId> 					m_eligibleInstances;
			std::unordered_map<std::size_t, SWorkerLoad> 		m_workerLoads;
			std::unordered_map<ContentsInstanceId, SEntryLoad> 	m_entryLoads;
			std::optional<Clock::time_point> 					m_previousEvaluation;
			std::optional<std::size_t> 							m_source;
			std::optional<SDelegateMove> 						m_move;
			std::optional<SDelegateSubmitResult> 				m_rejected;
			std::optional<SDelegateCandidate> 					m_reverse;
			Clock::duration 									m_verificationTime{};
			std::size_t 										m_observedWindows = 0;
			std::size_t 										m_recoveryCount = 0;
		};
	}
}
