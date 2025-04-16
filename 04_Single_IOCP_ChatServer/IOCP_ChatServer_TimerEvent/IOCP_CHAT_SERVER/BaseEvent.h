#pragma once

// Event ������ �ʿ��ϴٸ� BaseEvent�� ��ӹް� execute �� ���
// execute �� ���ڸ� ���� ��츦 ����Ͽ� ���� �Լ��� ������ ����

struct BaseEvent
{
	std::function<void()> execute;															// �̺�Ʈ �Լ��� �����ϰ� ���⿡ �Լ� �����͸� ����
};

struct TimerEvent : public BaseEvent
{
	bool	isPQCS;																			// PQCS�� IOCP ť�� �ѱ� Event ���� Ȥ�� ���� ���� ���������� �ѱ� ������
	DWORD	timeMs;
	DWORD	nextExecuteTime;
};

struct TimerEventComparator
{
	bool operator()(const TimerEvent *e1, const TimerEvent *e2) const noexcept
	{
		return e1->nextExecuteTime > e2->nextExecuteTime;									// ����ð� ���� ���� �켱
	}
};