#pragma once

// struct TestEvent : public TimerEvent
// {
// 	void SetEvent(DWORD ms, INT num);
// 	void Execute(INT num, INT delayFrame);
// };


struct SendAllTimer : public TimerEvent
{
	void SetEvent();
	// TimerEvent��(��) ���� ��ӵ�
	void execute(int delayFrame) override;
};