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

			node->link = top;
			top = node;
		}
	}

	__forceinline ~CObjectPool()
	{
		// FreeList에 있는 것 삭제
		while (top != nullptr)
		{
			if (m_bPlacementNewFlag)
			{
				top->data.~DATA();
			}

			Node *delNode = top;
			top = top->link;
			free(delNode);
		}

	}

	__forceinline DATA *Alloc()
	{
		Node *node;

		if (top == nullptr)
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
			node = top;
			top = top->link;

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

		nodePtr->link = top;
		top = nodePtr;
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

private:
	// FreeList
	// AllocFunc allocFunc;
	bool m_bPlacementNewFlag;
	int m_iSize = 0;
	Node *top = nullptr;
};
