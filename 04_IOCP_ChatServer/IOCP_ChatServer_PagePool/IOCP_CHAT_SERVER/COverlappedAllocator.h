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

	// (�� OVERLAPPED �����ּ� - VirtualAlloc �Ҵ� ���� �ּ�) / sizeof(OVERLAPPED) = �� OverlappedInfo �迭 ���� �ε���
	inline int CalIndex(ULONG_PTR myOverlappedPtr)
	{
		int infoStartIndex = (myOverlappedPtr - (ULONG_PTR)m_pOverlappeds) / sizeof(OVERLAPPED);
		return infoStartIndex;
	}

	inline ULONG_PTR GetStartAddr() { return (ULONG_PTR)m_pOverlappeds; }

	// �Ҵ� ������ ����
	// Overlapped ����ü 32����Ʈ
	// VirtualAlloc �ѹ���
	// 32 * 2048 �� ���� 682�� ���� �� ����
	// ����Ǯ 20000���� �ϸ� 20000 / 682 = ���� 30
private:
	// 64KB�� 1���� ��Ŷ���� ����
	static constexpr int MAX_BUCKET_SIZE = 30;

	LONG m_lSessionIndex = -1;
	char *m_pOverlappeds = nullptr;

public:
	OverlappedInfo m_infos[(64 * 1024 * MAX_BUCKET_SIZE) / sizeof(OVERLAPPED)];
};

extern COverlappedAllocator g_OverlappedAlloc;

