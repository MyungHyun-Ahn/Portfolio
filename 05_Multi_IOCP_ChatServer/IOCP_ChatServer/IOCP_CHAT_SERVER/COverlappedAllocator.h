#pragma once

// ������ ����ü�� �ִ��� �������� ���� ����ϰ� ���Ƴֱ� ���� �Ҵ��
// - �Ҵ� ������ ���� ������� ����

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
		// �ּ� �ִ� ��ŷ�� ����
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

	// (�� OVERLAPPED �����ּ� - VirtualAlloc �Ҵ� ���� �ּ�) / sizeof(OVERLAPPED) * 3 = �� OverlappedInfo �迭 ���� �ε���
	inline int GetAcceptExIndex(ULONG_PTR myOverlappedPtr) const noexcept
	{
		int AcceptExIndex = m_arrAcceptExIndex[(myOverlappedPtr - (ULONG_PTR)m_pOverlappeds) / (sizeof(OVERLAPPED) * 3)];
		return AcceptExIndex;
	}

	inline void SetAcceptExIndex(ULONG_PTR myOverlappedPtr, int index) noexcept
	{
		m_arrAcceptExIndex[(myOverlappedPtr - (ULONG_PTR)m_pOverlappeds) / (sizeof(OVERLAPPED) * 3)] = index;
	}

	// (�� OVERLAPPED �����ּ� - VirtualAlloc �Ҵ� ���� �ּ�) % (sizeof(OVERLAPPED) * 3) / sizeof(OVERLAPPED)
	// - 0 : AcceptEx
	// - 1 : Recv
	// - 2 : Send
	inline IOOperation CalOperation(ULONG_PTR myOverlappedPtr) const noexcept
	{
		IOOperation oper = (IOOperation)((myOverlappedPtr - (ULONG_PTR)m_pOverlappeds) % (sizeof(OVERLAPPED) * 3) / sizeof(OVERLAPPED));
		return oper;
	}

private:
	// 64KB�� 1���� ��Ŷ���� ����
	// ó�� �Ҵ��� �� ����� �޸� ������ �Ҵ� ���� ����
	// MAX_BUCKET_SIZE * 64KB�� ������ �Ҵ�
	static constexpr int MAX_BUCKET_SIZE = 30;

	LONG m_lSessionIndex = -1;
	char *m_pOverlappeds = nullptr;
	LONG m_arrAcceptExIndex[(64 * 1024 * MAX_BUCKET_SIZE) / (sizeof(OVERLAPPED) * 3)];
};

extern COverlappedAllocator g_OverlappedAlloc;

