#pragma once

// ������ ����ü�� �ִ��� �������� ���� ����ϰ� ���Ƴֱ� ���� �Ҵ��
// - �Ҵ� ������ ���� ������� ����

enum class IOOperation
{
	CONNECTEX,
	RECV,
	SEND,
	// System Event - 3 ~
	SENDPOST,
	RELEASE_SESSION,
	// Timer Event - 100 ~
	TIMER_EVENT = 100,
	CONTENT_EVENT = 200,
	NONE = 400
};

class COverlappedAllocator
{
public:
	COverlappedAllocator() noexcept
	{
		m_pOverlappeds = (char *)VirtualAlloc(nullptr, 64 * 1024 * MAX_BUCKET_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	}

	OVERLAPPED *Alloc() noexcept
	{
		LONG index = InterlockedIncrement(&m_lSessionIndex);
		OVERLAPPED *retOverlapped;
		retOverlapped = (OVERLAPPED *)(m_pOverlappeds + (sizeof(OVERLAPPED) * 3 * index));

		return retOverlapped;
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
};

extern COverlappedAllocator g_OverlappedAlloc;

