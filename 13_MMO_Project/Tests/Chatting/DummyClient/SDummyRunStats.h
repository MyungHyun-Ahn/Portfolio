#pragma once

namespace Chatting
{
	enum class EChatPhase
	{
		Idle, ConnectRequested, Connected, LoginPending, SectorPending, Ready, Closing, Terminal
	};

	inline constexpr std::array kPhaseNames{
		"idle", "connect_requested", "connected", "login_pending", "sector_pending", "ready", "closing", "terminal"};

	struct SDummyFailure
	{
		const char *m_pReason;
		INT64 m_accountNo;
		EChatPhase m_phase;
		DWORD m_error;
	};

	// Tick을 호출하는 앱 스레드에서만 갱신한다.
	struct SDummyRunStats
	{
		inline static constexpr std::array<UINT64, 24> kMessageRttBucketUpperMicroseconds{
			100, 250, 500, 1000, 2000, 3000, 4000, 5000, 7500, 10000, 15000, 20000,
			30000, 40000, 50000, 75000, 100000, 150000, 250000, 500000, 1000000, 2000000, 3000000, 5000000};

		std::array<UINT64, kPhaseNames.size()> m_stages{};
		UINT64 m_moveSubmitted = 0;
		UINT64 m_moveCompleted = 0;
		UINT64 m_messageSubmitted = 0;
		UINT64 m_messageCompleted = 0;
		UINT64 m_messageRttMinMicroseconds = 0;
		UINT64 m_messageRttMaxMicroseconds = 0;
		UINT64 m_messageRttSumMicroseconds = 0;
		std::array<UINT64, kMessageRttBucketUpperMicroseconds.size() + 1> m_messageRttBuckets{};
		UINT64 m_peerReceived = 0;
		UINT64 m_heartbeatSubmitted = 0;
		UINT64 m_reconnectRequested = 0;
		UINT64 m_reconnectStarted = 0;
		UINT64 m_reconnectReady = 0;
		UINT64 m_failureCount = 0;
		std::array<SDummyFailure, 16> m_failures{};

		// 정상 자기 응답만 모으며 건별 표본은 저장하지 않는다.
		void RecordMessageRtt(UINT64 elapsedMicroseconds) noexcept
		{
			if (m_messageCompleted == 0 || elapsedMicroseconds < m_messageRttMinMicroseconds)
				m_messageRttMinMicroseconds = elapsedMicroseconds;
			if (elapsedMicroseconds > m_messageRttMaxMicroseconds)
				m_messageRttMaxMicroseconds = elapsedMicroseconds;
			m_messageRttSumMicroseconds += elapsedMicroseconds;
			size_t index = 0;
			while (index < kMessageRttBucketUpperMicroseconds.size() && elapsedMicroseconds > kMessageRttBucketUpperMicroseconds[index])
				++index;
			++m_messageRttBuckets[index];
			++m_messageCompleted;
		}

		// 백분위는 해당 구간의 상한으로 보고한다. 마지막 구간은 실측 최댓값을 사용한다.
		UINT64 GetMessageRttPercentileMicroseconds(UINT64 percentile) const noexcept
		{
			const UINT64 rank = (m_messageCompleted * percentile + 99) / 100;
			UINT64 count = 0;
			for (size_t index = 0; index < kMessageRttBucketUpperMicroseconds.size(); ++index)
			{
				count += m_messageRttBuckets[index];
				if (count >= rank)
					return (std::min)(kMessageRttBucketUpperMicroseconds[index], m_messageRttMaxMicroseconds);
			}
			return m_messageRttMaxMicroseconds;
		}

		// 워커 합류 후에만 호출한다. 샘플을 자르더라도 전체 실패 건수는 유지한다.
		void Merge(const SDummyRunStats &stats) noexcept
		{
			for (size_t index = 0; index < m_stages.size(); ++index)
				m_stages[index] += stats.m_stages[index];
			m_moveSubmitted += stats.m_moveSubmitted;
			m_moveCompleted += stats.m_moveCompleted;
			m_messageSubmitted += stats.m_messageSubmitted;
			if (stats.m_messageCompleted != 0 && (m_messageCompleted == 0 || stats.m_messageRttMinMicroseconds < m_messageRttMinMicroseconds))
				m_messageRttMinMicroseconds = stats.m_messageRttMinMicroseconds;
			if (stats.m_messageRttMaxMicroseconds > m_messageRttMaxMicroseconds)
				m_messageRttMaxMicroseconds = stats.m_messageRttMaxMicroseconds;
			m_messageRttSumMicroseconds += stats.m_messageRttSumMicroseconds;
			for (size_t index = 0; index < m_messageRttBuckets.size(); ++index)
				m_messageRttBuckets[index] += stats.m_messageRttBuckets[index];
			m_messageCompleted += stats.m_messageCompleted;
			m_peerReceived += stats.m_peerReceived;
			m_heartbeatSubmitted += stats.m_heartbeatSubmitted;
			m_reconnectRequested += stats.m_reconnectRequested;
			m_reconnectStarted += stats.m_reconnectStarted;
			m_reconnectReady += stats.m_reconnectReady;
			for (size_t index = 0; m_failureCount + index < m_failures.size() && index < stats.m_failureCount; ++index)
				m_failures[static_cast<size_t>(m_failureCount) + index] = stats.m_failures[index];
			m_failureCount += stats.m_failureCount;
		}

		void Fail(const char *pReason, INT64 accountNo = 0, EChatPhase phase = EChatPhase::Idle, DWORD error = 0) noexcept
		{
			if (m_failureCount < m_failures.size())
				m_failures[static_cast<size_t>(m_failureCount)] = {pReason, accountNo, phase, error};
			++m_failureCount;
		}
	};
}
