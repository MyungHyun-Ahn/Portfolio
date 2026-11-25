#pragma once

namespace ContentsRuntimeLib
{
	class CContentsRuntime;
	struct SEntryTimings;

	struct SDelegateCandidate
	{
		ContentsInstanceId 	instanceId;
		std::size_t 		destinationWorkerIndex;
	};

	enum class EDelegateSubmitStatus
	{
		NotSubmitted,	// 후보가 스케줄러 자신
		Rejected,		// RequestDelegate()가 false를 반환함 → 다음 선정에서 같은 인스턴스·목적지 쌍만 제외
		Accepted		// RequestDelegate()가 true를 반환함 → 접수만 된 상태라 실제 편입을 기다림
	};

	// 직전 후보의 제출 결과는 편입 여부와 별개다.
	struct SDelegateSubmitResult
	{
		SDelegateCandidate 		candidate;
		EDelegateSubmitStatus 	status;
	};

	class CContentsScheduler final : public IContents
	{
	public:
		using SelectCandidate = std::function<std::optional<SDelegateCandidate>(const SEntryTimings &,
			const std::optional<SDelegateSubmitResult> &)>;

		// 기존 워커에서 평가할 주기와 후보 선택 정책을 지정한다.
		CContentsScheduler(ContentsInstanceId instanceId, CContentsRuntime &runtime,
			std::chrono::milliseconds evaluationInterval, SelectCandidate selectCandidate);

	protected:
		// 평가 주기가 되면 후보 하나를 기존 인계 경로에 요청한다.
		void OnFrame() override;

	private:
		CContentsRuntime 						&m_runtime;
		const std::chrono::milliseconds 		m_evaluationInterval;
		SelectCandidate 						m_selectCandidate;
		std::optional<SDelegateSubmitResult> 	m_lastSubmit;
		std::optional<std::chrono::steady_clock::time_point> 	m_lastEvaluationTime;
	};
}
