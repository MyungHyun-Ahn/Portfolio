#pragma once
// �ý������� �̺�Ʈ�� ���⼭ ����

struct MonitorTimerEvent : public TimerEvent
{
	void SetEvent() noexcept;
	void Execute() noexcept;
};

struct SendAllTimerEvent : public TimerEvent
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

	// frame 1�ʿ� ��� ����Ǿ�� ����
	void SetEvent(CBaseContent *pBaseContent, int frame) noexcept;
	void Execute(int delayFrame) noexcept;

	CBaseContent *m_pBaseContent;
};

struct EnqueuePacketEvent : public BaseEvent
{
	void SetEvent(UINT64 sessionId, CSerializableBuffer<FALSE> *sBuffer) noexcept;
	void Execute(UINT64 sessionId, CSerializableBuffer<FALSE> *sBuffer, int delayFrame);

};