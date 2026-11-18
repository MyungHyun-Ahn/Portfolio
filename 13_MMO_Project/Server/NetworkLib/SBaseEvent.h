#pragma once

namespace NetworkLib
{
// Event 수행이 필요하다면 BaseEvent를 상속받고 execute 를 등록
struct SBaseEvent
{
	SBaseEvent(bool isTimer = false) : m_isTimerEvent(isTimer) {}
	bool m_isTimerEvent;
	virtual void Execute(int delayFrame) = 0;
};

struct STimerEvent : public SBaseEvent
{
	STimerEvent()
		: SBaseEvent(true)
		, m_isRunning(true)
	{
	}

	bool	m_isRunning;
	DWORD	m_timeMilliseconds;
	DWORD	m_nextExecuteTime;
};

struct STimerEventComparator
{
	bool operator()(const STimerEvent *pE1, const STimerEvent *pE2) const noexcept
	{
		return pE1->m_nextExecuteTime > pE2->m_nextExecuteTime; // 실행시간 빠른 것을 우선
	}
};
}
