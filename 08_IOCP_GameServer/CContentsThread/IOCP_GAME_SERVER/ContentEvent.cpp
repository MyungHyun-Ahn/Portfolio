#include "pch.h"
#include "CNetServer.h"
#include "ContentEvent.h"

// void TestEvent::SetEvent(DWORD ms, INT num)
// {
// 	isTimerEvent = true;
// 
// 	execute = std::bind(&TestEvent::Execute, this, num, std::placeholders::_1);
// 	timeMs = ms; // 1초
// 	nextExecuteTime = timeGetTime(); // 현재 시각
// }
// 
// void TestEvent::Execute(INT num, INT delayFrame)
// {
// 	Sleep(19);
// 	// printf("%d\n", num);
// }

void ContentsThreadTest::SetEvent(int frame)
{
	if (frame == 0)
		timeMs = 0;
	else
		timeMs = 1000 / frame;

	nextExecuteTime = timeGetTime();
}

DISABLE_OPTIMIZATION
void ContentsThreadTest::execute(int delayFrame)
{
	for (int cnt = 0; cnt < delayFrame; cnt++)
	{
		int sum = 0;
		for (int i = 0; i < 1024 * 20; i++)
		{
			sum += i;
		}

		InterlockedIncrement(&m_FPS);
	}
}
ENABLE_OPTIMIZATION
