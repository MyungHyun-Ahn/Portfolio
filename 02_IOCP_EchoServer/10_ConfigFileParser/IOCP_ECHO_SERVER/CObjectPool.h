#pragma once

// #define SAFE_MODE

template<typename DATA>
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

template<typename DATA>
class CObjectPool
{
	using Node = Node<DATA>;
	typedef Node *(CObjectPool<DATA>:: *AllocFunc)(void);
public:
	__forceinline CObjectPool(int initCount, bool placementNewFlag)
		: m_iSize(0), m_bPlacementNewFlag(placementNewFlag)
	{
		InitializeSRWLock(&s_srwLock);

		// initCount 만큼 FreeList 할당
		for (int i = 0; i < initCount; i++)
		{
			Node *node;
			if (m_bPlacementNewFlag)
			{
				node = PlacementNewAlloc();
			}
			else
			{
				node = NewAlloc();
			}

			node->link = m_top;
			m_top = node;
		}
	}

	__forceinline ~CObjectPool()
	{
		// FreeList에 있는 것 삭제
		while (m_top != nullptr)
		{
			if (m_bPlacementNewFlag)
			{
				m_top->data.~DATA();
			}

			Node *delNode = m_top;
			m_top = m_top->link;
			free(delNode);
		}

	}

	__forceinline DATA *Alloc()
	{
		Node *node;

		if (m_top == nullptr)
		{
			if (m_bPlacementNewFlag)
			{
				node = PlacementNewAlloc();
			}
			else
			{
				node = NewAlloc();
			}
		}
		else
		{
			node = m_top;
			m_top = m_top->link;

			if (m_bPlacementNewFlag)
			{
				node = new (node) Node;
			}
		}

		// data의 주소를 반환
		return &node->data;
	}
	__forceinline void Free(DATA *ptr)
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

		if (m_bPlacementNewFlag)
		{
			nodePtr->data.~DATA();
		}

		nodePtr->link = m_top;
		m_top = nodePtr;
	}

private:
	__forceinline Node *PlacementNewAlloc()
	{
		Node *mallocNode = (Node *)malloc(sizeof(Node));
		Node *newNode = new (mallocNode) Node;
#ifdef SAFE_MODE
		newNode->poolPtr = this;
#endif
		m_iSize++;
		return newNode;
	}

	__forceinline Node *NewAlloc()
	{
		Node *newNode = new Node;
#ifdef SAFE_MODE
		newNode->poolPtr = this;
#endif
		m_iSize++;
		return newNode;
	}

public:
	inline static void Lock()
	{
		AcquireSRWLockExclusive(&s_srwLock);
	}

	inline static void UnLock()
	{
		ReleaseSRWLockExclusive(&s_srwLock);
	}

private:
	// FreeList
	// AllocFunc allocFunc;
	bool		m_bPlacementNewFlag;
	int			m_iSize = 0;
	Node		*m_top = nullptr;
	inline static SRWLOCK		s_srwLock;
};
