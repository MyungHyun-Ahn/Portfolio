#pragma once

// Event ������ �ʿ��ϴٸ� BaseEvent�� ��ӹް� execute �� ���
// execute �� ���ڸ� ���� ��츦 ����Ͽ� ���� �Լ��� ������ ����
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
		return e1->nextExecuteTime > e2->nextExecuteTime; // ����ð� ���� ���� �켱
	}
};

