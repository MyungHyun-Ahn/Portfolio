#pragma once
// �ý������� �̺�Ʈ�� ���⼭ ����

struct MonitorEvent : public BaseEvent
{
	void SetEvent() noexcept;
	void Execute() noexcept;
};

struct KeyBoardEvent : public BaseEvent
{
	void SetEvent() noexcept;
	void Execute() noexcept;
};