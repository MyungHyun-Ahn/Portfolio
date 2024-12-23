#include <stdio.h>
#include <windows.h>
#include <process.h>
#include <new>
#include "LFDefine.h"
#include "CLFMemoryPool.h"
#include "CTLSSharedMemoryPool.h"
#include "CTLSMemoryPool.h"
#include "CLFStack.h"
#include "CProfileManager.h"
#include "CLFQueue.h"
#include <queue>

// 스레드 2개로 해야 분석이 편함
#define THREAD_COUNT 4
#define TEST_LOOP_COUNT 100000
#define ENQUEUE_DEQUEUE_COUNT 128

CLFQueue<UINT64, FALSE> lockfreeQueue1;
CLFQueue<UINT64, TRUE> lockfreeQueue2;

std::queue<UINT64> stdQueue;

unsigned int CAS01ThreadFunc(LPVOID lpParam)
{
	for (int i = 0; i < TEST_LOOP_COUNT; i++)
	// while (1)
	{
		PROFILE_BEGIN(1, "ThreadFunc");

		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
		{
			lockfreeQueue1.Enqueue(i);

		}

		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
		{
			UINT64 data;
			lockfreeQueue1.Dequeue(&data);
		}
	}

	return 0;
}

unsigned int CAS02ThreadFunc(LPVOID lpParam)
{
	for (int i = 0; i < TEST_LOOP_COUNT; i++)
		// while (1)
	{
		PROFILE_BEGIN(1, "ThreadFunc");

		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
		{
			lockfreeQueue2.Enqueue(i);

		}

		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
		{
			UINT64 data;
			lockfreeQueue2.Dequeue(&data);
		}
	}

	return 0;
}

SRWLOCK lock;

unsigned int stdQueueThreadFunc(LPVOID lpParam)
{
	for (int i = 0; i < TEST_LOOP_COUNT; i++)
		// while (1)
	{
		PROFILE_BEGIN(1, "ThreadFunc");

		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
		{
			PROFILE_BEGIN(1, "Enqueue");
			AcquireSRWLockExclusive(&lock);
			stdQueue.push(i);
			ReleaseSRWLockExclusive(&lock);
		}

		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
		{
			PROFILE_BEGIN(1, "Dequeue");
			AcquireSRWLockExclusive(&lock);
			UINT64 data;
			data = stdQueue.front();
			stdQueue.pop();
			ReleaseSRWLockExclusive(&lock);
		}
	}

	return 0;
}

#define KB 1024

// TLS 메모리 풀 테스트
class LargeBuffer
{
public:
	BYTE buffer[1 * KB];
};

CLFMemoryPool<LargeBuffer> lfMemoryPool = CLFMemoryPool<LargeBuffer>(0, false);

unsigned int LFMemoryPoolThreadFunc(LPVOID lpParam)
{
	for (int i = 0; i < TEST_LOOP_COUNT; i++)
		// while (1)
	{
		PROFILE_BEGIN(1, nullptr);

		// Alloc
		LargeBuffer *Bufs[ENQUEUE_DEQUEUE_COUNT];

		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
		{
			// PROFILE_BEGIN(1, "Alloc");
			Bufs[i] = lfMemoryPool.Alloc();
		}

		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
		{
			// PROFILE_BEGIN(1, "Free");
			lfMemoryPool.Free(Bufs[i]);
		}
	}

	return 0;
}

CTLSMemoryPoolManager<LargeBuffer> tlsMemoryPool = CTLSMemoryPoolManager<LargeBuffer>();

unsigned int TLSMemoryPoolThreadFunc(LPVOID lpParam)
{
	for (int i = 0; i < TEST_LOOP_COUNT; i++)
		// while (1)
	{
		PROFILE_BEGIN(1, nullptr);

		// Alloc
		LargeBuffer *Bufs[ENQUEUE_DEQUEUE_COUNT];

		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
		{
			// PROFILE_BEGIN(1, "Alloc");
			Bufs[i] = tlsMemoryPool.Alloc();
		}

		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
		{
			// PROFILE_BEGIN(1, "Free");
			tlsMemoryPool.Free(Bufs[i]);
		}
	}

	return 0;
}

unsigned int NewDeleteThreadFunc(LPVOID lpParam)
{
	for (int i = 0; i < TEST_LOOP_COUNT; i++)
		// while (1)
	{
		PROFILE_BEGIN(1, nullptr);

		// Alloc
		LargeBuffer *Bufs[ENQUEUE_DEQUEUE_COUNT];

		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
		{
			// PROFILE_BEGIN(1, "Alloc");
			Bufs[i] = new LargeBuffer;
		}

		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
		{
			// PROFILE_BEGIN(1, "Free");
			delete Bufs[i];
		}
	}

	return 0;
}


int main()
{
	g_ProfileMgr = CProfileManager::GetInstance();

	InitializeSRWLock(&lock);

	HANDLE arrTh[THREAD_COUNT];
	// for (int i = 0; i < THREAD_COUNT; i++)
	// {
	// 	arrTh[i] = (HANDLE)_beginthreadex(nullptr, 0, LFMemoryPoolThreadFunc, nullptr, CREATE_SUSPENDED, nullptr);
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

	for (int i = 0; i < THREAD_COUNT; i++)
	{
		arrTh[i] = (HANDLE)_beginthreadex(nullptr, 0, TLSMemoryPoolThreadFunc, nullptr, CREATE_SUSPENDED, nullptr);
		if (arrTh[i] == 0)
			return 1;
	}

	for (int i = 0; i < THREAD_COUNT; i++)
	{
		ResumeThread(arrTh[i]);
	}

	WaitForMultipleObjects(THREAD_COUNT, arrTh, true, INFINITE);

	for (int i = 0; i < THREAD_COUNT; i++)
	{
		arrTh[i] = (HANDLE)_beginthreadex(nullptr, 0, NewDeleteThreadFunc, nullptr, CREATE_SUSPENDED, nullptr);
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