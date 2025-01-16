#include <stdio.h>
#include <windows.h>
#include <process.h>
#include <new>
#include "LFDefine.h"
#include "CLFMemoryPool.h"
#include "CLFStack.h"
#include "CProfileManager.h"
#include "CLFQueue.h"
#include <queue>

// 스레드 2개로 해야 분석이 편함
#define THREAD_COUNT 2
#define TEST_LOOP_COUNT 500000
#define ENQUEUE_DEQUEUE_COUNT 64

// class Test
// {
// 	int val;
// };

// CLFQueue<UINT64, FALSE> lockfreeQueue1;
// CLFQueue<UINT64, TRUE> lockfreeQueue2;
// 
// std::queue<UINT64> stdQueue;
// 
// unsigned int CAS01ThreadFunc(LPVOID lpParam)
// {
// 	for (int i = 0; i < TEST_LOOP_COUNT; i++)
// 	// while (1)
// 	{
// 		PROFILE_BEGIN(1, "ThreadFunc");
// 
// 		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
// 		{
// 			lockfreeQueue1.Enqueue(i);
// 
// 		}
// 
// 		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
// 		{
// 			UINT64 data;
// 			lockfreeQueue1.Dequeue(&data);
// 		}
// 	}
// 
// 	return 0;
// }
// 
// unsigned int CAS02ThreadFunc(LPVOID lpParam)
// {
// 	for (int i = 0; i < TEST_LOOP_COUNT; i++)
// 		// while (1)
// 	{
// 		PROFILE_BEGIN(1, "ThreadFunc");
// 
// 		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
// 		{
// 			lockfreeQueue2.Enqueue(i);
// 
// 		}
// 
// 		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
// 		{
// 			UINT64 data;
// 			lockfreeQueue2.Dequeue(&data);
// 		}
// 	}
// 
// 	return 0;
// }
// 
// SRWLOCK lock;
// 
// unsigned int stdQueueThreadFunc(LPVOID lpParam)
// {
// 	for (int i = 0; i < TEST_LOOP_COUNT; i++)
// 		// while (1)
// 	{
// 		PROFILE_BEGIN(1, "ThreadFunc");
// 
// 		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
// 		{
// 			PROFILE_BEGIN(1, "Enqueue");
// 			AcquireSRWLockExclusive(&lock);
// 			stdQueue.push(i);
// 			ReleaseSRWLockExclusive(&lock);
// 		}
// 
// 		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
// 		{
// 			PROFILE_BEGIN(1, "Dequeue");
// 			AcquireSRWLockExclusive(&lock);
// 			UINT64 data;
// 			data = stdQueue.front();
// 			stdQueue.pop();
// 			ReleaseSRWLockExclusive(&lock);
// 		}
// 	}
// 
// 	return 0;
// }


CLFQueue<std::pair<DWORD, UINT64>> lockfreeQueueTest;

unsigned int TestFunc(LPVOID lpParam)
{
	UINT64 value = 0;
	UINT64 prevValue = 0;

	while (true)
	{
		for (int i = 0; i < 2; i++)
		{
			lockfreeQueueTest.Enqueue(std::make_pair(GetCurrentThreadId(), ++value));
		}

		for (int i = 0; i < 2; i++)
		{
			std::pair<DWORD, UINT64> pair;
			lockfreeQueueTest.Dequeue(&pair);

			if (pair.first == GetCurrentThreadId())
			{
				if (prevValue >= pair.second)
				{
					__debugbreak();
				}
				else
				{
					prevValue = pair.second;
				}
			}
		}
	}
}


int main()
{
	// g_ProfileMgr = CProfileManager::GetInstance();
	// 
	// InitializeSRWLock(&lock);
	// 
	// HANDLE arrTh[THREAD_COUNT];
	// for (int i = 0; i < THREAD_COUNT; i++)
	// {
	// 	arrTh[i] = (HANDLE)_beginthreadex(nullptr, 0, CAS01ThreadFunc, nullptr, CREATE_SUSPENDED, nullptr);
	// 	if (arrTh[i] == 0)
	// 		return 1;
	// }
	// 
	// for (int i = 0; i < THREAD_COUNT; i++)
	// {
	// 	ResumeThread(arrTh[i]);
	// }
	// 
	// WaitForMultipleObjects(THREAD_COUNT, arrTh, true, INFINITE);
	// 
	// for (int i = 0; i < THREAD_COUNT; i++)
	// {
	// 	arrTh[i] = (HANDLE)_beginthreadex(nullptr, 0, CAS02ThreadFunc, nullptr, CREATE_SUSPENDED, nullptr);
	// 	if (arrTh[i] == 0)
	// 		return 1;
	// }
	// 
	// for (int i = 0; i < THREAD_COUNT; i++)
	// {
	// 	ResumeThread(arrTh[i]);
	// }
	// 
	// WaitForMultipleObjects(THREAD_COUNT, arrTh, true, INFINITE);
	// 
	// for (int i = 0; i < THREAD_COUNT; i++)
	// {
	// 	arrTh[i] = (HANDLE)_beginthreadex(nullptr, 0, stdQueueThreadFunc, nullptr, CREATE_SUSPENDED, nullptr);
	// 	if (arrTh[i] == 0)
	// 		return 1;
	// }

	HANDLE arrTh[THREAD_COUNT];
	for (int i = 0; i < THREAD_COUNT; i++)
	{
		arrTh[i] = (HANDLE)_beginthreadex(nullptr, 0, TestFunc, nullptr, CREATE_SUSPENDED, nullptr);
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