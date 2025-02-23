#include "pch.h"
#include "CNetServer.h"
#include "ContentEvent.h"

void TestEvent::SetEvent(DWORD ms, INT num)
{
	isTimerEvent = true;

	execute = std::bind(&TestEvent::Execute, this, num, std::placeholders::_1);
	timeMs = ms; // 1��
	nextExecuteTime = timeGetTime(); // ���� �ð�
}

void TestEvent::Execute(INT num, INT delayFrame)
{
	Sleep(18);
	// printf("%d\n", num);
}
