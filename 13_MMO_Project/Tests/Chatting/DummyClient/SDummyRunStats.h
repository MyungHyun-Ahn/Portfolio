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
		std::array<UINT64, kPhaseNames.size()> m_stages{};
		UINT64 m_moveSubmitted = 0;
		UINT64 m_moveCompleted = 0;
		UINT64 m_messageSubmitted = 0;
		UINT64 m_messageCompleted = 0;
		UINT64 m_peerReceived = 0;
		UINT64 m_heartbeatSubmitted = 0;
		UINT64 m_reconnectRequested = 0;
		UINT64 m_reconnectStarted = 0;
		UINT64 m_reconnectReady = 0;
		UINT64 m_failureCount = 0;
		std::array<SDummyFailure, 16> m_failures{};

		// 워커 합류 후에만 호출한다. 샘플을 자르더라도 전체 실패 건수는 유지한다.
		void Merge(const SDummyRunStats &stats) noexcept
		{
			for (size_t index = 0; index < m_stages.size(); ++index)
				m_stages[index] += stats.m_stages[index];
			m_moveSubmitted += stats.m_moveSubmitted;
			m_moveCompleted += stats.m_moveCompleted;
			m_messageSubmitted += stats.m_messageSubmitted;
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
