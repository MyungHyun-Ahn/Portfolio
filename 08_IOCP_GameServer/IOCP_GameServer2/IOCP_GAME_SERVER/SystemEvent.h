#pragma once
// �ý������� �̺�Ʈ�� ���⼭ ����

struct MonitorTimerEvent : public TimerEvent
{
	void SetEvent() noexcept;
	void execute(int delayFrame) noexcept override;
};

struct SendAllTimerEvent : public TimerEvent
{
	void SetEvent() noexcept;
	void execute(int delayFrame) noexcept override;
};

struct KeyBoardTimerEvent : public TimerEvent
{
	void SetEvent() noexcept;
	void execute(int delayFrame) noexcept override;
};

class CBaseContents;

struct ContentsFrameEvent : public TimerEvent
{
	~ContentsFrameEvent();

	// frame 1�ʿ� ��� ����Ǿ�� ����
	void SetEvent(CBaseContents *pBaseContent, int frame) noexcept;
	void execute(int delayFrame) noexcept override;

	CBaseContents *m_pBaseContent;
};

struct ExampleTimerEvent : public TimerEvent
{
	void SetEvent(int param1, double param2, int frame) noexcept;
	void execute(int delayFrame) noexcept override;

	int m_iParam1;
	double m_dParam2;
	// ...
};