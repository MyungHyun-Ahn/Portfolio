#include "ContentsRuntimeLibPch.h"
#include "IContents.h"
#include "CContentsRuntime.h"
#include "CContentsScheduler.h"

namespace ContentsRuntimeLib
{
	CContentsScheduler::CContentsScheduler(ContentsInstanceId instanceId, CContentsRuntime &runtime,
		std::chrono::milliseconds evaluationInterval, SelectCandidate selectCandidate)
		: IContents(instanceId)
		, m_runtime(runtime)
		, m_evaluationInterval(evaluationInterval)
		, m_selectCandidate(std::move(selectCandidate))
	{
	}

	void CContentsScheduler::OnFrame()
	{
		// 1. 정책이 없거나 평가 주기가 유효하지 않으면 건너뛴다.
		if (!m_selectCandidate || m_evaluationInterval <= std::chrono::milliseconds::zero())
			return;

		// 2. 첫 주기에는 기준 시각만 설정한다.
		using Clock = std::chrono::steady_clock;
		const auto now = Clock::now();
		if (!m_lastEvaluationTime)
		{
			m_lastEvaluationTime = now;
			return;
		}

		// 3. 평가 주기가 되지 않았으면 건너뛴다.
		if (std::chrono::duration_cast<std::chrono::milliseconds>(now - *m_lastEvaluationTime) < m_evaluationInterval)
			return;

		// 4. 직전 제출 결과를 한 번 넘기고 새 후보의 접수 결과를 남긴다.
		const auto lastSubmit = std::exchange(m_lastSubmit, std::nullopt);
		const auto candidate = m_selectCandidate(m_runtime.TakeEntryTimings(), lastSubmit);
		if (candidate)
		{
			auto status = EDelegateSubmitStatus::NotSubmitted;
			if (candidate->instanceId != GetInstanceId())
			{
				const bool accepted = m_runtime.RequestDelegate(candidate->instanceId, candidate->destinationWorkerIndex);
				status = accepted ? EDelegateSubmitStatus::Accepted : EDelegateSubmitStatus::Rejected;
			}
			m_lastSubmit = SDelegateSubmitResult{*candidate, status};
		}
		m_lastEvaluationTime = Clock::now();
	}
}
