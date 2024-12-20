#include <stdio.h>
#include <windows.h>
#include <process.h>
#include <new>
#include "LFDefine.h"
#include "CLFMemoryPool.h"
#include "CLFStack.h"
#include "CLFQueue.h"

// 스레드 2개로 해야 분석이 편함
#define THREAD_COUNT 2

CLFQueue<UINT64> lockfreeQueue;

unsigned int ThreadFunc(LPVOID lpParam)
{
	while (1)
	{
		int thId = GetCurrentThreadId();

		for (UINT64 i = 0; i < 3; i++)
		{
			lockfreeQueue.Enqueue(i);

		}

		for (UINT64 i = 0; i < 3; i++)
		{
			UINT64 data;
			lockfreeQueue.Dequeue(&data);
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
}