#pragma once

namespace ContentsRuntimeLib
{
	class CContentsRuntime;
	struct SWorkerTimingSnapshot;

	struct SDelegateCandidate
	{
		ContentsInstanceId instanceId;
		std::size_t destinationWorkerIndex;
	};

	class CContentsScheduler final : public IContents
	{
	public:
		using SelectCandidate = std::function<std::optional<SDelegateCandidate>(const std::vector<SWorkerTimingSnapshot> &)>;

		// 기존 워커에서 평가할 주기와 후보 선택 정책을 지정한다.
		CContentsScheduler(ContentsInstanceId instanceId, CContentsRuntime &runtime,
			std::chrono::milliseconds evaluationInterval, SelectCandidate selectCandidate);

	protected:
		// 평가 주기가 되면 후보 하나를 기존 인계 경로에 요청한다.
		void OnFrame() override;

	private:
		CContentsRuntime &m_runtime;
		const std::chrono::milliseconds m_evaluationInterval;
		SelectCandidate m_selectCandidate;
		std::optional<std::chrono::steady_clock::time_point> m_lastEvaluationTime;
	};
}
