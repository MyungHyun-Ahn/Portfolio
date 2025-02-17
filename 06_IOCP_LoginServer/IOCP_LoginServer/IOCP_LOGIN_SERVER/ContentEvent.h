#pragma once
struct HeartBeatEvent : public TimerEvent
{
	void SetEvent() noexcept;
	void Execute() noexcept;
};