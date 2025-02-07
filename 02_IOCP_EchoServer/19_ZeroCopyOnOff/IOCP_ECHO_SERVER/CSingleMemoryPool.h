#pragma once
// #define SAFE_MODE // 메모리 풀에서 할당한 것인지 판별 용도

template<typename DATA>
class CSingleMemoryPool
{
public:
	struct Node
	{
#ifdef SAFE_MODE
		void *poolPtr;
#endif
		DATA data;
		Node *link;

#ifdef SAFE_MODE
		int safeWord = 0xFFFF;
#endif
	};

public:
	__forceinline CSingleMemoryPool(int initCount = 0) noexcept
		: m_iUseCount(0), m_iCapacity(0)
	{
		// initCount 만큼 FreeList 할당
		for (int i = 0; i < initCount; i++)
		{
			Node *node = NewAlloc();
			node->link = top;
			top = node;
		}
	}

	__forceinline DATA *Alloc() noexcept
	{
		Node *node;

		if (top == nullptr)
		{
			node = NewAlloc();
		}
		else
		{
			node = top;
			top = top->link;
		}

		m_iUseCount++;
		// data의 주소를 반환
		return &node->data;
	}
	__forceinline void Free(DATA *ptr) noexcept
	{
#ifdef SAFE_MODE
		unsigned __int64 intPtr = (__int64)ptr;
		intPtr -= sizeof(void *);
		Node *nodePtr = (Node *)intPtr;

		if (nodePtr->poolPtr != this)
			throw;

		if (nodePtr->safeWord != 0xFFFF)
			throw;
#else
		Node *nodePtr = (Node *)ptr;
#endif
		nodePtr->link = top;
		top = nodePtr;

		m_iUseCount--;
	}

	__forceinline LONG GetCapacity() noexcept { return m_iCapacity; }
	__forceinline LONG GetUseCount() noexcept { return m_iUseCount; }

private:
	__forceinline Node *NewAlloc() noexcept
	{
		Node *newNode = new Node;
#ifdef SAFE_MODE
		newNode->poolPtr = this;
#endif
		m_iCapacity++;
		return newNode;
	}

private:
	// FreeList
	int m_iUseCount = 0;
	int m_iCapacity = 0;
	Node *top = nullptr;
};