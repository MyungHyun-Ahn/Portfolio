#pragma once

// Event 수행이 필요하다면 BaseEvent를 상속받고 execute 를 등록
// execute 에 인자를 넣을 경우를 대비하여 가상 함수로 만들지 않음
struct BaseEvent
{
	BaseEvent(bool isTimer = false) : isTimerEvent(isTimer) {}
	bool isTimerEvent;
	virtual void execute(int delayFrame) = 0;
};

struct TimerEvent : public BaseEvent
{
	TimerEvent() : BaseEvent(true), isRunning(true) {}

	bool	isRunning;
	DWORD	timeMs;
	DWORD	nextExecuteTime;
};

struct TimerEventComparator
{
	bool operator()(const TimerEvent *e1, const TimerEvent *e2) const noexcept
	{
		return e1->nextExecuteTime > e2->nextExecuteTime; // 실행시간 빠른 것을 우선
	}
};

