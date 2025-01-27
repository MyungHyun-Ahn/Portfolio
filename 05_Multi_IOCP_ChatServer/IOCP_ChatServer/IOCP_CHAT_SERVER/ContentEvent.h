#pragma once
struct SectorBroadcastEvent : public BaseEvent
{
	void SetEvent() noexcept;
	void Execute() noexcept;
};

struct NonLoginHeartBeatEvent : public BaseEvent
{
	void SetEvent() noexcept;
	void Execute() noexcept;
};

struct LoginHeartBeatEvent : public BaseEvent
{
	void SetEvent() noexcept;
	void Execute() noexcept;
};