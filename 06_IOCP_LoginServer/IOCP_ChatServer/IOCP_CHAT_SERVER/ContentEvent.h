#pragma once
struct SectorBroadcastTimerEvent : public TimerEvent
{
	void SetEvent() noexcept;
	void Execute() noexcept;
};

struct NonLoginHeartBeatTimerEvent : public TimerEvent
{
	void SetEvent() noexcept;
	void Execute() noexcept;
};

struct LoginHeartBeatTimerEvent : public TimerEvent
{
	void SetEvent() noexcept;
	void Execute() noexcept;
};