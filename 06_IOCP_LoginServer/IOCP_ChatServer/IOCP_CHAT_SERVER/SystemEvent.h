#pragma once
// �ý������� �̺�Ʈ�� ���⼭ ����

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

struct OnAcceptEvent : public BaseEvent
{
	void SetEvent(const UINT64 sessionID) noexcept;
	void Execute(const UINT64 sessionID) noexcept;
};

struct OnClientLeaveEvent : public BaseEvent
{
	void SetEvent(const UINT64 sessionID) noexcept;
	void Execute(const UINT64 sessionID) noexcept;
};

struct SerializableBufferFreeEvent : public BaseEvent
{
	void SetEvent(CDeque<CSerializableBuffer<SERVER_TYPE::WAN> *> *freeQueue) noexcept;
	void Execute(CDeque<CSerializableBuffer<SERVER_TYPE::WAN> *> *freeQueue) noexcept;
};