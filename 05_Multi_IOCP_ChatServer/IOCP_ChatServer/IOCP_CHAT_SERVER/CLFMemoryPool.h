#pragma once

#pragma warning(disable : 6001)
// #define SAFE_MODE

template<typename DATA>
struct MemoryPoolNode
{
#ifdef SAFE_MODE
	void *poolPtr;
#endif
	MemoryPoolNode() noexcept = default;

	DATA data;
	ULONG_PTR next;

#ifdef SAFE_MODE
	int safeWord = 0xFFFF;
#endif
};

// placement 지원 안함
template<typename DATA>
class CLFMemoryPool
{
	using Node = MemoryPoolNode<DATA>;

public:
	// 이 부분은 싱글 스레드에서 진행
	// - 다른 스레드가 생성되고는 절대 초기화를 진행하면 안됨
	__forceinline CLFMemoryPool(int initCount = 0) noexcept
		: m_iCapacity(0)
	{
		// initCount 만큼 FreeList 할당
		for (int i = 0; i < initCount; i++)
		{
			ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
			Node *newTop = NewAlloc();
			ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)newTop);
			newTop->next = m_pTop;
			m_pTop = combinedNewTop;
		}
	}

	// 다른 스레드가 모두 해제된 이후에 진행됨
	__forceinline ~CLFMemoryPool() noexcept
	{
		// FreeList에 있는 것 삭제
		while (m_pTop != NULL)
		{
			Node *delNode = (Node *)GetAddress(m_pTop);
			m_pTop = delNode->next;
			free(delNode);
		}

	}

	// Pop 행동
	DATA *Alloc() noexcept
	{
		Node *node;

		LONG size = InterlockedDecrement(&m_iUseCount);
		if (size < 0)
		{
			InterlockedIncrement(&m_iUseCount);
			node = NewAlloc();
			return &node->data;
		}

		ULONG_PTR readTop;
		ULONG_PTR newTop;

		do
		{
			readTop = m_pTop;
			if (readTop == NULL)
			{
				node = NewAlloc();
				break;
			}

			node = (Node *)GetAddress(readTop);
			newTop = node->next;
		} while (InterlockedCompareExchange(&m_pTop, newTop, readTop) != readTop);

		// InterlockedIncrement(&m_iUseCount);

		// data의 주소를 반환
		return &node->data;
	}

	// 스택으로 치면 Push 행동
	void Free(DATA *ptr) noexcept
	{
#ifdef SAFE_MODE
		unsigned __int64 intPtr = (__int64)ptr;
		intPtr -= sizeof(void *);
		Node *newTop = (Node *)intPtr;

		if (newTop->poolPtr != this)
			throw;

		if (newTop->safeWord != 0xFFFF)
			throw;
#else
		Node *newTop = (Node *)ptr;
#endif
		ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
		ULONG_PTR readTop;
		ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)newTop);

		do
		{
			readTop = m_pTop;
			newTop->next = readTop;

		} while (InterlockedCompareExchange(&m_pTop, combinedNewTop, readTop) != readTop);

		// Push 성공
		InterlockedIncrement(&m_iUseCount);
	}

private:
	__forceinline Node *NewAlloc() noexcept
	{
		Node *newNode = new Node;
#ifdef SAFE_MODE
		newNode->poolPtr = this;
#endif
		InterlockedIncrement(&m_iCapacity);
		return newNode;
	}

public:
	inline LONG GetCapacity() const noexcept { return m_iCapacity; }
	inline LONG GetUseCount() const noexcept { return m_iUseCount; }

private:
	LONG		m_iUseCount = 0;
	LONG		m_iCapacity = 0;
	ULONG_PTR	m_ullCurrentIdentifier = 0;
	ULONG_PTR	m_pTop = 0;
};
