#pragma once

// 오버랩 구조체를 최대한 페이지를 적게 사용하고 몰아넣기 위한 할당기
// - 할당 해제는 전혀 고려하지 않음

enum class IOOperation
{
	ACCEPTEX,
	RECV,
	SEND,
	SENDPOST,
	SECTOR_BROADCAST,
	RELEASE_SESSION,
	NONE = 400
};

class COverlappedAllocator
{
public:
	COverlappedAllocator() noexcept
	{
		int err;
		// 최소 최대 워킹셋 수정
		SetProcessWorkingSetSize(GetCurrentProcess(), 1000 * 1024 * 1024, (size_t)(10000) * 1024 * 1024);
		m_pOverlappeds = (char *)VirtualAlloc(nullptr, 64 * 1024 * MAX_BUCKET_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (VirtualLock(m_pOverlappeds, 64 * 1024 * MAX_BUCKET_SIZE) == FALSE)
		{
			err = GetLastError();
			__debugbreak();
		}
	}

	OVERLAPPED *Alloc() noexcept
	{
		LONG index = InterlockedIncrement(&m_lSessionIndex);
		OVERLAPPED *retOverlapped;
		retOverlapped = (OVERLAPPED *)(m_pOverlappeds + (sizeof(OVERLAPPED) * 3 * index));

		return retOverlapped;
	}

	// (내 OVERLAPPED 시작주소 - VirtualAlloc 할당 시작 주소) / sizeof(OVERLAPPED) * 3 = 내 OverlappedInfo 배열 시작 인덱스
	inline int GetAcceptExIndex(ULONG_PTR myOverlappedPtr) const noexcept
	{
		int AcceptExIndex = m_arrAcceptExIndex[(myOverlappedPtr - (ULONG_PTR)m_pOverlappeds) / (sizeof(OVERLAPPED) * 3)];
		return AcceptExIndex;
	}

	inline void SetAcceptExIndex(ULONG_PTR myOverlappedPtr, int index) noexcept
	{
		m_arrAcceptExIndex[(myOverlappedPtr - (ULONG_PTR)m_pOverlappeds) / (sizeof(OVERLAPPED) * 3)] = index;
	}

	// (내 OVERLAPPED 시작주소 - VirtualAlloc 할당 시작 주소) % (sizeof(OVERLAPPED) * 3) / sizeof(OVERLAPPED)
	// - 0 : AcceptEx
	// - 1 : Recv
	// - 2 : Send
	inline IOOperation CalOperation(ULONG_PTR myOverlappedPtr) const noexcept
	{
		IOOperation oper = (IOOperation)((myOverlappedPtr - (ULONG_PTR)m_pOverlappeds) % (sizeof(OVERLAPPED) * 3) / sizeof(OVERLAPPED));
		return oper;
	}

private:
	// 64KB를 1개의 버킷으로 간주
	// 처음 할당할 때 연결된 메모리 영역을 할당 받을 것임
	// MAX_BUCKET_SIZE * 64KB의 영역을 할당
	static constexpr int MAX_BUCKET_SIZE = 30;

	LONG m_lSessionIndex = -1;
	char *m_pOverlappeds = nullptr;
	LONG m_arrAcceptExIndex[(64 * 1024 * MAX_BUCKET_SIZE) / (sizeof(OVERLAPPED) * 3)];
};

extern COverlappedAllocator g_OverlappedAlloc;

