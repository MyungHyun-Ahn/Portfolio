#pragma once

// struct TestEvent : public TimerEvent
// {
// 	void SetEvent(DWORD ms, INT num);
// 	void Execute(INT num, INT delayFrame);
// };


struct SendAllTimer : public TimerEvent
{
	void SetEvent();
	// TimerEvent을(를) 통해 상속됨
	void execute(int delayFrame) override;
};