#pragma once

// Timer Event
// DB �α� ����
struct DBTimerEvent : public TimerEvent
{
	void SetEvent() noexcept;
	void Excute() noexcept;

	int mon = 0;
};