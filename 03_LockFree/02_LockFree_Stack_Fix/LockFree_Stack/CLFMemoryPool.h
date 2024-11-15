#pragma once

// #define SAFE_MODE

template<typename DATA>
struct MemoryPoolNode
{
#ifdef SAFE_MODE
	void *poolPtr;
#endif
	MemoryPoolNode() = default;

	DATA data;
	ULONG_PTR next;

#ifdef SAFE_MODE
	int safeWord = 0xFFFF;
#endif
};

template<typename DATA>
class CLFMemoryPool
{
	using Node = MemoryPoolNode<DATA>;
	
public:
	// 이 부분은 싱글 스레드에서 진행
	// - 다른 스레드가 생성되고는 절대 초기화를 진행하면 안됨
	__forceinline CLFMemoryPool(int initCount, bool placementNewFlag)
		: m_iSize(0), m_bPlacementNewFlag(placementNewFlag)
	{
		// initCount 만큼 FreeList 할당
		for (int i = 0; i < initCount; i++)
		{
			ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
			Node *newTop;
			if (m_bPlacementNewFlag)
			{
				newTop = PlacementNewAlloc();
			}
			else
			{
				newTop = NewAlloc();
			}

			ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)newTop);
			newTop->next = m_pTop;
			m_pTop = combinedNewTop;
		}
	}

	// 다른 스레드가 모두 해제된 이후에 진행됨
	__forceinline ~CLFMemoryPool()
	{
		// FreeList에 있는 것 삭제
		while (m_pTop != NULL)
		{
			Node *delNode = (Node *)GetAddress(m_pTop);
			if (m_bPlacementNewFlag)
			{
				delNode->data.~DATA();
			}

			m_pTop = delNode->next;
			free(delNode);
		}

	}

	// Pop 행동
	__forceinline DATA *Alloc()
	{
		Node *node;

		// nullptr을 보고 들어오면 그냥 할당하고 반환
		// - 이 시점에 Free 노드가 생겨도 그냥 할당해도 됨
		if (m_pTop == NULL)
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
			ULONG_PTR readTop;
			ULONG_PTR newTop;

			do 
			{
				readTop = m_pTop;
				if (readTop == NULL)
				{
					if (m_bPlacementNewFlag)
					{
						node = PlacementNewAlloc();
					}
					else
					{
						node = NewAlloc();
					}

					break;
				}

				node = (Node *)GetAddress(readTop);
				newTop = node->next;
			} while (InterlockedCompareExchange(&m_pTop, newTop, readTop) != readTop);

			// 반환될 노드가 결정된 이후 진행
			if (m_bPlacementNewFlag)
			{
				node = new (node) Node;
			}
		}

		// data의 주소를 반환
		return &node->data;
	}

	// 스택으로 치면 Push 행동
	__forceinline void Free(DATA *ptr)
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
		// nodePtr 소멸자 호출
		// 먼저 진행되어야 함
		if (m_bPlacementNewFlag)
		{
			newTop->data.~DATA();
		}

		ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
		ULONG_PTR readTop;
		ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)newTop);

		do 
		{
			readTop = m_pTop;
			newTop->next = readTop;

		} while (InterlockedCompareExchange(&m_pTop, combinedNewTop, readTop) != readTop);

		// Push 성공
	}

private:
	__forceinline Node *PlacementNewAlloc()
	{
		Node *mallocNode = (Node *)malloc(sizeof(Node));
		Node *newNode = new (mallocNode) Node;
#ifdef SAFE_MODE
		newNode->poolPtr = this;
#endif
		InterlockedIncrement(&m_iSize);
		return newNode;
	}

	__forceinline Node *NewAlloc()
	{
		Node *newNode = new Node;
#ifdef SAFE_MODE
		newNode->poolPtr = this;
#endif
		InterlockedIncrement(&m_iSize);
		return newNode;
	}

private:
	bool		m_bPlacementNewFlag;
	LONG		m_iSize = 0;
	ULONG_PTR	m_ullCurrentIdentifier = 0;
	ULONG_PTR	m_pTop = 0;
};