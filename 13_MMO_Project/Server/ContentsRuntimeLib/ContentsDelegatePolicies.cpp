#include "ContentsRuntimeLibPch.h"
#include "IContents.h"
#include "CContentsRuntime.h"
#include "CContentsScheduler.h"
#include "ContentsDelegatePolicies.h"
#include <algorithm>
#include <cmath>

namespace ContentsRuntimeLib::DelegatePolicies
{
	namespace
	{
		constexpr double kOverload = 0.80;
		constexpr double kRecovery = 0.65;
		constexpr double kDestinationLoad = 0.50;
		constexpr double kDestinationLimit = 0.75;
		constexpr double kMinimumGain = 0.10;
		constexpr double kLoadEpsilon = 1e-9;
	}

	CPersistentOverloadDelegatePolicy::CPersistentOverloadDelegatePolicy(SPersistentOverloadSettings settings,
		std::vector<ContentsInstanceId> eligibleInstances)
		: m_settings(settings)
		, m_eligibleInstances(std::move(eligibleInstances))
	{
	}

	std::optional<SDelegateCandidate> CPersistentOverloadDelegatePolicy::operator()(const SEntryTimings &timings,
		const std::optional<SDelegateSubmitResult> &lastSubmit)
	{
		return EvaluateAt(timings, Clock::now(), lastSubmit);
	}

	std::optional<SDelegateCandidate> CPersistentOverloadDelegatePolicy::EvaluateAt(const SEntryTimings &snapshot, Clock::time_point now,
		const std::optional<SDelegateSubmitResult> &lastSubmit)
	{
		// 1. 정책 설정과 평가 시각을 확인하고 첫 호출에서는 기준 시각만 잡는다.
		const auto &timings = snapshot.entries;
		if (m_eligibleInstances.empty() || m_settings.observationPeriod.count() <= 0 || m_settings.minimumWindows == 0)
			return std::nullopt;

		if (m_previousEvaluation && now <= *m_previousEvaluation)
			return std::nullopt;

		const auto previous = std::exchange(m_previousEvaluation, now);
		if (!previous)
			return std::nullopt;

		// 2. 직전 인계 상태를 확인하고 관찰에 쓸 수 없는 구간은 버린다.
		if (!canObserve(snapshot, lastSubmit))
		{
			clearObservation();
			return std::nullopt;
		}

		// 3. 완료 방문 시간으로 워커 부하를 계산하고 관찰 중이면 누적한다.
		const auto elapsed = now - *previous;
		for (std::size_t i = 0; i < snapshot.workerCount; ++i)
		{
			auto &worker = m_workerLoads[i];
			worker.latest.reset();
			worker.latestTime = {};
			worker.hasEntry = false;
		}
		for (const auto &timing : timings)
		{
			auto &worker = m_workerLoads.at(timing.workerIndex);
			// 완료 표본이 없어도 엔트리가 있으면 빈 워커가 아니다.
			worker.hasEntry = true;
			if (timing.visitCount == 0 || timing.totalTime < Clock::duration::zero())
				continue;

			worker.latest = 0.0;
			worker.latestTime += timing.totalTime;
			if (m_source == timing.workerIndex && std::ranges::find(m_eligibleInstances, timing.instanceId) != m_eligibleInstances.end())
			{
				auto &entry = m_entryLoads[timing.instanceId];
				entry.totalTime += timing.totalTime;
				++entry.observedCount;
			}
		}
		for (auto &[index, worker] : m_workerLoads)
		{
			if (!worker.hasEntry)
				worker.latest = 0.0;

			if (worker.latest)
			{
				worker.latest = std::chrono::duration<double>(worker.latestTime) / elapsed;
				if (m_source || m_move)
				{
					worker.totalTime += worker.latestTime;
					worker.elapsed += elapsed;
					++worker.observedCount;
					if (!worker.hasEntry)
						++worker.emptyCount;
				}
			}
			if (!m_source && !m_move)
				worker.highCount = worker.latest && *worker.latest >= kOverload ? (std::min)(worker.highCount + 1, std::size_t{2}) : 0;
		}

		// 4. 편입 후 검증 중이면 새 후보를 찾지 않고 양쪽 부하를 확인한다.
		if (m_move)
		{
			verifyDelegate(elapsed);
			return std::nullopt;
		}

		// 5. 80% 이상이 두 구간 연속인 워커 중 부하가 가장 높은 곳의 관찰을 시작한다.
		if (!m_source)
		{
			for (const auto &[index, worker] : m_workerLoads)
			{
				if (worker.highCount != 2)
					continue;

				if (!m_source || *worker.latest > *m_workerLoads.at(*m_source).latest
					|| (worker.latest == m_workerLoads.at(*m_source).latest && index < *m_source))
					m_source = index;
			}
			return std::nullopt;
		}

		// 6. 회복하거나 미관측이면 관찰을 끝내고, 아니면 관찰 기간과 구간 수를 채운다.
		const auto source = *m_source;
		const auto &worker = m_workerLoads.at(source);
		++m_observedWindows;
		m_recoveryCount = worker.latest && *worker.latest <= kRecovery ? m_recoveryCount + 1 : 0;
		if (!worker.latest || m_recoveryCount == 2)
		{
			clearObservation();
			return std::nullopt;
		}

		if (worker.elapsed < m_settings.observationPeriod || m_observedWindows < m_settings.minimumWindows)
			return std::nullopt;

		// 7. 누적 부하와 마지막 부하가 모두 높으면 인계 후보를 고른다.
		std::optional<SDelegateCandidate> candidate;
		if (*worker.latest >= kOverload && std::chrono::duration<double>(worker.totalTime) / worker.elapsed >= kOverload)
		{
			candidate = selectCandidate(source, timings);
			if (m_rejected && m_entryLoads.contains(m_rejected->candidate.instanceId))
				m_rejected.reset();

			if (m_reverse && m_entryLoads.contains(m_reverse->instanceId))
				m_reverse.reset();
		}

		if (candidate)
			m_move = SDelegateMove{*candidate, source, EDelegateStage::Proposed};
		clearObservation();
		return candidate;
	}

	bool CPersistentOverloadDelegatePolicy::canObserve(const SEntryTimings &snapshot,
		const std::optional<SDelegateSubmitResult> &lastSubmit)
	{
		// 1. 직전 후보의 제출 결과로 접수 대기 상태를 갱신한다.
		if (m_move && m_move->stage == EDelegateStage::Proposed)
		{
			const auto &candidate = m_move->candidate;
			if (!lastSubmit || lastSubmit->candidate.instanceId != candidate.instanceId
				|| lastSubmit->candidate.destinationWorkerIndex != candidate.destinationWorkerIndex)
			{
				// 제출을 확인하지 못했으면 판단을 다시 시작한다. 전환 표본은 이후에도 제외한다.
				m_move.reset();
				return false;
			}

			if (lastSubmit->status == EDelegateSubmitStatus::Accepted)
				m_move->stage = EDelegateStage::Accepted;
			else
			{
				m_rejected = lastSubmit;
				m_move.reset();
				return false;
			}
		}
		// 2. 실제 워커 범위를 벗어난 행이 있으면 표본을 사용하지 않는다.
		const auto &timings = snapshot.entries;
		for (const auto &timing : timings)
		{
			if (timing.workerIndex >= snapshot.workerCount)
				return false;
		}

		// 3. 편입을 기다리고 목적지 실행 목록에 들어간 뒤에만 검증 단계로 바꾼다.
		if (m_move && m_move->stage == EDelegateStage::Accepted)
		{
			const auto entry = std::ranges::find(timings, m_move->candidate.instanceId, &SEntryTimingSnapshot::instanceId);
			if (entry == timings.end() || entry->isDelegating)
				return false;
			// 목적지 큐에 들어간 상태와 실행 목록 편입을 구분한다.
			if (entry->workerIndex == m_move->candidate.destinationWorkerIndex)
			{
				m_reverse = SDelegateCandidate{m_move->candidate.instanceId, m_move->source};
				m_move->stage = EDelegateStage::Verifying;
			}
			else
				m_move.reset();
			return false;
		}
		// 4. 인계 전후 배정이 섞일 수 있는 평가 구간은 제외한다.
		for (const auto &timing : timings)
		{
			if (timing.didRequestDelegate || timing.isDelegating)
				return false;
		}
		return true;
	}

	void CPersistentOverloadDelegatePolicy::verifyDelegate(Clock::duration elapsed)
	{
		// 1. 편입 후 새 표본으로 검증 기간과 구간 수를 채운다.
		m_verificationTime += elapsed;
		++m_observedWindows;
		if (m_verificationTime < m_settings.observationPeriod || m_observedWindows < m_settings.minimumWindows)
			return;

		// 2. 양쪽 워커의 표본이 모두 충분한지 확인한다.
		const auto sourceIndex = m_move->source;
		const auto destinationIndex = m_move->candidate.destinationWorkerIndex;
		m_move.reset();
		// 한쪽이라도 미관측이면 효과를 확정하지 않고 일반 발견 단계로 돌아간다.
		if (m_workerLoads.at(sourceIndex).observedCount != m_observedWindows
			|| m_workerLoads.at(destinationIndex).observedCount != m_observedWindows)
		{
			clearObservation();
			return;
		}

		// 3. 과부하가 남은 워커 중 부하가 더 높은 곳을 다시 관찰한다.
		std::optional<std::size_t> source;
		double highest = 0.0;
		for (const auto index : {sourceIndex, destinationIndex})
		{
			const auto &worker = m_workerLoads.at(index);
			if (*worker.latest < kOverload)
				continue;

			const double load = std::chrono::duration<double>(worker.totalTime) / worker.elapsed;
			if (load >= kOverload && load > highest)
			{
				source = index;
				highest = load;
			}
		}

		clearObservation();
		m_source = source;
	}

	std::optional<SDelegateCandidate> CPersistentOverloadDelegatePolicy::selectCandidate(std::size_t source,
		const std::vector<SEntryTimingSnapshot> &timings) const
	{
		// 1. 출발 워커의 관찰 부하를 기준으로 인계 이득을 비교한다.
		const auto &sourceWindow = m_workerLoads.at(source);
		const double sourceLoad = std::chrono::duration<double>(sourceWindow.totalTime) / sourceWindow.elapsed;
		double bestGain = 0.0;
		double bestDestinationLoad = 0.0;
		std::optional<SDelegateCandidate> best;
		// 2. 관찰 구간을 모두 채운 출발지 엔트리의 예상 인계 비용을 계산한다.
		for (const auto &candidate : timings)
		{
			const auto entry = m_entryLoads.find(candidate.instanceId);
			if (candidate.workerIndex != source || entry == m_entryLoads.end() || entry->second.observedCount != m_observedWindows)
				continue;

			const double candidateLoad = std::chrono::duration<double>(entry->second.totalTime) / sourceWindow.elapsed;
			if (candidateLoad <= 0.0 || candidateLoad > sourceLoad)
				continue;

			// 3. 관찰 근거와 여유가 충분하고 재시도 제한에 걸리지 않는 목적지만 비교한다.
			for (const auto &[i, destination] : m_workerLoads)
			{
				if (i == source || !destination.latest || destination.observedCount != m_observedWindows
					|| (destination.emptyCount != 0 && destination.emptyCount != m_observedWindows))
					continue;

				if (m_rejected && candidate.instanceId == m_rejected->candidate.instanceId
					&& (m_rejected->status == EDelegateSubmitStatus::NotSubmitted || i == m_rejected->candidate.destinationWorkerIndex))
					continue;

				if (m_reverse && candidate.instanceId == m_reverse->instanceId && i == m_reverse->destinationWorkerIndex)
					continue;

				const double load = std::chrono::duration<double>(destination.totalTime) / destination.elapsed;
				if (*destination.latest > kDestinationLoad || load > kDestinationLoad || load + candidateLoad > kDestinationLimit)
					continue;

				// 4. 인계 전후 두 워커의 최대 부하를 비교해 개선 폭이 가장 큰 조합을 고른다.
				const double gain = (std::max)(sourceLoad, load) - (std::max)(sourceLoad - candidateLoad, load + candidateLoad);
				if (gain + kLoadEpsilon < kMinimumGain)
					continue;
					
				const bool tied = std::abs(gain - bestGain) <= kLoadEpsilon;
				const bool betterTie = best && tied && (load < bestDestinationLoad || (load == bestDestinationLoad
					&& std::pair{candidate.instanceId, i} < std::pair{best->instanceId, best->destinationWorkerIndex}));
				if (!best || gain > bestGain + kLoadEpsilon || betterTie)
				{
					best = SDelegateCandidate{candidate.instanceId, i};
					bestGain = gain;
					bestDestinationLoad = load;
				}
			}
		}
		return best;
	}

	void CPersistentOverloadDelegatePolicy::clearObservation()
	{
		m_workerLoads.clear();
		m_entryLoads.clear();
		m_source.reset();
		m_observedWindows = 0;
		m_recoveryCount = 0;
		m_verificationTime = {};
	}
}
