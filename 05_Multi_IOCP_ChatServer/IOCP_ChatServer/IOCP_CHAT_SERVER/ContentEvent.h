#pragma once
struct SectorBroadcastEvent : public TimerEvent
{
	void SetEvent() noexcept;
	void Execute() noexcept;
};

struct NonLoginHeartBeatEvent : public TimerEvent
{
	void SetEvent() noexcept;
	void Execute() noexcept;
};

struct LoginHeartBeatEvent : public TimerEvent
{
	void SetEvent() noexcept;
	void Execute() noexcept;
};