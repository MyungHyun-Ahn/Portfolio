#pragma once
// 시스템적인 이벤트는 여기서 선언

struct MonitorTimerEvent : public TimerEvent
{
	void SetEvent() noexcept;
	void Execute() noexcept;
};

struct KeyBoardTimerEvent : public TimerEvent
{
	void SetEvent() noexcept;
	void Execute() noexcept;
};