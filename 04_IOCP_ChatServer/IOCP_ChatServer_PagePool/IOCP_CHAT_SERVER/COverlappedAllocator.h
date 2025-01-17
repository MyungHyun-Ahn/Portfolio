#pragma once

enum class IOOperation
{
	ACCEPTEX,
	RECV,
	SEND
};


struct OverlappedInfo
{
	IOOperation		operation;
	LONG			index;
};

struct OverlappedBucket
{
	char *Overlappeds = nullptr;
	OverlappedInfo infos[(64 * 1024) / sizeof(OVERLAPPED)];
};

class COverlappedAllocator
{
public:
	COverlappedAllocator() noexcept
	{
		m_pOverlappeds = (char *)VirtualAlloc(nullptr, 64 * 1024 * MAX_BUCKET_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	}

	void Alloc(OVERLAPPED **OverlappedPtr) noexcept
	{
		LONG index = InterlockedIncrement(&m_lSessionIndex);
		*OverlappedPtr = (OVERLAPPED *)(m_pOverlappeds + (32 * 3 * index));
	}

	// (내 OVERLAPPED 시작주소 - VirtualAlloc 할당 시작 주소) / sizeof(OVERLAPPED) = 내 OverlappedInfo 배열 시작 인덱스
	inline int CalIndex(ULONG_PTR myOverlappedPtr)
	{
		int infoStartIndex = (myOverlappedPtr - (ULONG_PTR)m_pOverlappeds) / sizeof(OVERLAPPED);
		return infoStartIndex;
	}

	inline ULONG_PTR GetStartAddr() { return (ULONG_PTR)m_pOverlappeds; }

	// 할당 해제는 안함
	// Overlapped 구조체 32바이트
	// VirtualAlloc 한번에
	// 32 * 2048 총 세션 682개 넣을 수 있음
	// 세션풀 20000개라 하면 20000 / 682 = 대충 30
private:
	// 64KB를 1개의 버킷으로 간주
	static constexpr int MAX_BUCKET_SIZE = 30;

	LONG m_lSessionIndex = -1;
	char *m_pOverlappeds = nullptr;

public:
	OverlappedInfo m_infos[(64 * 1024 * MAX_BUCKET_SIZE) / sizeof(OVERLAPPED)];
};

extern COverlappedAllocator g_OverlappedAlloc;

