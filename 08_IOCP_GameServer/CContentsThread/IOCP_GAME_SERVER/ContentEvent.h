#pragma once

// struct TestEvent : public TimerEvent
// {
// 	void SetEvent(DWORD ms, INT num);
// 	void Execute(INT num, INT delayFrame);
// };

struct ContentsThreadTest : public TimerEvent
{
	void SetEvent(int frame);
	void execute(int delayFrame) override;

	LONG m_FPS = 0;
};