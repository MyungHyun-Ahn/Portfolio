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
		if (!m_selectCandidate || m_evaluationInterval <= std::chrono::milliseconds::zero())
			return;

		using Clock = std::chrono::steady_clock;
		const auto now = Clock::now();
		if (!m_lastEvaluationTime)
		{
			m_lastEvaluationTime = now;
			return;
		}
		if (std::chrono::duration_cast<std::chrono::milliseconds>(now - *m_lastEvaluationTime) < m_evaluationInterval)
			return;

		const auto candidate = m_selectCandidate();
		if (candidate && candidate->instanceId != GetInstanceId())
			(void)m_runtime.RequestDelegate(candidate->instanceId, candidate->destinationWorkerIndex);
		m_lastEvaluationTime = Clock::now();
	}
}
