#pragma once
// 시스템적인 이벤트는 여기서 선언

struct MonitorEvent : public BaseEvent
{
	void SetEvent() noexcept;
	void Execute() noexcept;
};

struct KeyBoardEvent : public BaseEvent
{
	void SetEvent() noexcept;
	void Execute() noexcept;
};