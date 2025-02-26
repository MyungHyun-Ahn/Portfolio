#pragma once
// 시스템적인 이벤트는 여기서 선언

struct MonitorTimerEvent : public TimerEvent
{
	void SetEvent() noexcept;
	void Execute() noexcept;
};

struct KeyBoardTimerEvent : public TimerEvent
{
	void SetEvent() noexcept;
	void Execute() noexcept;
};

class CBaseContent;

struct ContentFrameEvent : public TimerEvent
{
	~ContentFrameEvent();

	// frame 1초에 몇번 수행되어야 할지
	void SetEvent(CBaseContent *pBaseContent, int frame) noexcept;
	void Execute(int delayFrame) noexcept;

	CBaseContent *m_pBaseContent;
};