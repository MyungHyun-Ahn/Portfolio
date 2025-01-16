#include "pch.h"
#include <queue>

#define THREAD_COUNT 4
#define TEST_LOOP_COUNT 100000
#define ENQUEUE_DEQUEUE_COUNT 128

#define KB 1024

// TLS 메모리 풀 테스트
class LargeBuffer
{
public:
	BYTE buffer[1 * KB];
};

CTLSMemoryPoolManager<LargeBuffer, 64, 2, false> lfMemoryPool = CTLSMemoryPoolManager<LargeBuffer, 64, 2, false>();

unsigned int StackTLSPool(LPVOID lpParam)
{
	for (int i = 0; i < TEST_LOOP_COUNT; i++)
		// while (1)
	{
		//PROFILE_BEGIN(1, nullptr);

		// Alloc
		LargeBuffer *Bufs[ENQUEUE_DEQUEUE_COUNT];

		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
		{
			PROFILE_BEGIN(1, "Alloc");
			Bufs[i] = lfMemoryPool.Alloc();
		}

		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
		{
			PROFILE_BEGIN(1, "Free");
			lfMemoryPool.Free(Bufs[i]);
		}
	}

	return 0;
}

CTLSMemoryPoolManager<LargeBuffer, 64, 2, true> tlsMemoryPool = CTLSMemoryPoolManager<LargeBuffer, 64, 2, true>();

unsigned int QueueTLSPool(LPVOID lpParam)
{
	for (int i = 0; i < TEST_LOOP_COUNT; i++)
		// while (1)
	{
		// PROFILE_BEGIN(1, nullptr);

		// Alloc
		LargeBuffer *Bufs[ENQUEUE_DEQUEUE_COUNT];

		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
		{
			PROFILE_BEGIN(1, "Alloc");
			Bufs[i] = tlsMemoryPool.Alloc();
		}

		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
		{
			PROFILE_BEGIN(1, "Free");
			tlsMemoryPool.Free(Bufs[i]);
		}
	}

	return 0;
}

unsigned int NewDelete(LPVOID lpParam)
{
	for (int i = 0; i < TEST_LOOP_COUNT; i++)
		// while (1)
	{
		// PROFILE_BEGIN(1, nullptr);

		// Alloc
		LargeBuffer *Bufs[ENQUEUE_DEQUEUE_COUNT];

		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
		{
			PROFILE_BEGIN(1, "Alloc");
			Bufs[i] = new LargeBuffer;
		}

		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
		{
			PROFILE_BEGIN(1, "Free");
			delete Bufs[i];
		}
	}

	return 0;
}

CTLSPagePoolManager<128, 2> pagePool;

unsigned int TestCode(LPVOID lpParam)
{
	while (true)
	{
		for (UINT64 i = 0; i < ENQUEUE_DEQUEUE_COUNT; i++)
		{
			ULONG_PTR ptrs[128];

			for (int i = 0; i < (128 * 1024) / 1024; i++)
			{
				ptrs[i] = (ULONG_PTR)pagePool.Alloc();
				if ((~ptrs[i] & 0b111) != 0b111)
					__debugbreak();
			}

			for (int i = 0; i < (128 * 1024) / 1024; i++)
			{
				pagePool.Free((void *)ptrs[i]);
			}
		}
	}

	return 0;
}

int main()
{
	g_ProfileMgr = CProfileManager::GetInstance();

	HANDLE arrTh[THREAD_COUNT];
	for (int i = 0; i < THREAD_COUNT; i++)
	{
		arrTh[i] = (HANDLE)_beginthreadex(nullptr, 0, TestCode, nullptr, CREATE_SUSPENDED, nullptr);
		if (arrTh[i] == 0)
			return 1;
	}
	
	for (int i = 0; i < THREAD_COUNT; i++)
	{
		ResumeThread(arrTh[i]);
	}
	
	WaitForMultipleObjects(THREAD_COUNT, arrTh, true, INFINITE);

	// for (int i = 0; i < THREAD_COUNT; i++)
	// {
	// 	arrTh[i] = (HANDLE)_beginthreadex(nullptr, 0, QueueTLSPool, nullptr, CREATE_SUSPENDED, nullptr);
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
	// 	arrTh[i] = (HANDLE)_beginthreadex(nullptr, 0, NewDelete, nullptr, CREATE_SUSPENDED, nullptr);
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

	printf("정상종료\n");

	return 0;
}