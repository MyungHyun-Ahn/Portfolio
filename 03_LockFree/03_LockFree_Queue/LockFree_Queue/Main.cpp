#include <stdio.h>
#include <windows.h>
#include <process.h>
#include <new>
#include "LFDefine.h"
#include "CLFMemoryPool.h"
#include "CLFStack.h"

// 스레드 2개로 해야 분석이 편함
#define THREAD_COUNT 2

CLFStack<UINT64> lockfreeStack;

unsigned int ThreadFunc(LPVOID lpParam)
{
	while (1)
	{
		int thId = GetCurrentThreadId();

		for (UINT64 i = 0; i < 1000; i++)
		{
			lockfreeStack.Push(i);

		}

		for (int i = 0; i < 1000; i++)
		{
			UINT64 data;
			lockfreeStack.Pop(&data);
			// printf("Pop ThreadId = %d, Data = %d\n", thId, data);
		}
	}
}

int main()
{
	HANDLE arrTh[THREAD_COUNT];
	for (int i = 0; i < THREAD_COUNT; i++)
	{
		arrTh[i] = (HANDLE)_beginthreadex(nullptr, 0, ThreadFunc, nullptr, CREATE_SUSPENDED, nullptr);
		if (arrTh[i] == 0)
			return 1;
	}

	for (int i = 0; i < THREAD_COUNT; i++)
	{
		ResumeThread(arrTh[i]);
	}

	WaitForMultipleObjects(THREAD_COUNT, arrTh, true, INFINITE);

	printf("정상종료\n");

	if (lockfreeStack.m_pTop != NULL)
		__debugbreak();
}