#pragma once

namespace NetworkLib
{
// 시스템적인 이벤트는 여기서 선언

struct SMonitorTimerEvent : public STimerEvent
{
	void SetEvent() noexcept;
	void Execute(int delayFrame) noexcept override;
};

struct SSendAllTimerEvent : public STimerEvent
{
	void SetEvent() noexcept;
	void Execute(int delayFrame) noexcept override;
};

struct SKeyboardTimerEvent : public STimerEvent
{
	void SetEvent() noexcept;
	void Execute(int delayFrame) noexcept override;
};

class CBaseContents;

struct SContentsFrameEvent : public STimerEvent
{
	~SContentsFrameEvent();

	// frame 1초에 몇번 수행되어야 할지
	void SetEvent(CBaseContents *pBaseContent, int frame) noexcept;
	void Execute(int delayFrame) noexcept override;

	CBaseContents *m_pBaseContent;
};

struct SExampleTimerEvent : public STimerEvent
{
	void SetEvent(int param1, double param2, int frame) noexcept;
	void Execute(int delayFrame) noexcept override;

	int m_param1;
	double m_dParam2;
	// ...
};
}
