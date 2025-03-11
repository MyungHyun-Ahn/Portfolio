#pragma once
// 시스템적인 이벤트는 여기서 선언

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

class CBaseContent;

struct ContentFrameEvent : public TimerEvent
{
	~ContentFrameEvent();

	// frame 1초에 몇번 수행되어야 할지
	void SetEvent(CBaseContent *pBaseContent, int frame) noexcept;
	void execute(int delayFrame) noexcept override;

	CBaseContent *m_pBaseContent;
};

struct EnqueuePacketEvent : public BaseEvent
{
	void SetEvent(UINT64 sessionId, CSerializableBuffer<FALSE> *sBuffer) noexcept;
	void execute(int delayFrame) noexcept override;
	// void Execute(UINT64 sessionId, CSerializableBuffer<FALSE> *sBuffer, int delayFrame);
	UINT64 SessionId;
	CSerializableBuffer<FALSE> *SBuffer;
};