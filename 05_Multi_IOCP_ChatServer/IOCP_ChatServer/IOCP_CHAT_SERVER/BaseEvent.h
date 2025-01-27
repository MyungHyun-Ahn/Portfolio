#pragma once

// Event ������ �ʿ��ϴٸ� BaseEvent�� ��ӹް� execute �� ���
// execute �� ���ڸ� ���� ��츦 ����Ͽ� ���� �Լ��� ������ ����
struct BaseEvent
{
	std::function<void()> execute;		// �̺�Ʈ �Լ��� �����ϰ� ���⿡ �Լ� �����͸� ����
	DWORD timeMs;
	DWORD nextExecuteTime;
};

struct EventComparator
{
	bool operator()(const BaseEvent *e1, const BaseEvent *e2) const noexcept
	{
		return e1->nextExecuteTime > e2->nextExecuteTime; // ����ð� ���� ���� �켱
	}
};