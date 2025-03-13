#pragma once

// Timer Event
// DB 로그 저장
struct DBTimerEvent : public TimerEvent
{
	void SetEvent() noexcept;
	void Excute() noexcept;

	int mon = 0;
};