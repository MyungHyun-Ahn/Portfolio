#include "pch.h"
#include "CNetServer.h"
#include "ContentEvent.h"
#include "CGameServer.h"

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

void SendAllTimer::SetEvent()
{
	isTimerEvent = true;
	timeMs = 1000 / 25;
	nextExecuteTime = timeGetTime();
}

void SendAllTimer::execute(int delayFrame)
{
	((CGameServer *)(NET_SERVER::g_NetServer))->SendEchoAll();
}
