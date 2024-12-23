#pragma once

// #define SAFE_MODE
#pragma warning(disable : 6001)

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
public:
	using Node = MemoryPoolNode<DATA>;

	// �� �κ��� �̱� �����忡�� ����
	// - �ٸ� �����尡 �����ǰ�� ���� �ʱ�ȭ�� �����ϸ� �ȵ�
	__forceinline CLFMemoryPool(int initCount, bool placementNewFlag) noexcept
		: m_iCapacity(0), m_bPlacementNewFlag(placementNewFlag)
	{
		// initCount ��ŭ FreeList �Ҵ�
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

	// �ٸ� �����尡 ��� ������ ���Ŀ� �����
	__forceinline ~CLFMemoryPool() noexcept
	{
		// FreeList�� �ִ� �� ����
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

	// Pop �ൿ
	__forceinline DATA *Alloc() noexcept
	{
		Node *node;

		// nullptr�� ���� ������ �׳� �Ҵ��ϰ� ��ȯ
		// - �� ������ Free ��尡 ���ܵ� �׳� �Ҵ��ص� ��
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

			// ��ȯ�� ��尡 ������ ���� ����
			if (readTop != NULL)
			{
				InterlockedDecrement(&m_iFreeCount);
			}
			
			if (m_bPlacementNewFlag)
			{
				node = new (node) Node;
			}
		}

		// data�� �ּҸ� ��ȯ
		return &node->data;
	}

	// �������� ġ�� Push �ൿ
	__forceinline void Free(DATA *ptr) noexcept
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
		// nodePtr �Ҹ��� ȣ��
		// ���� ����Ǿ�� ��
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

		// Push ����
		InterlockedIncrement(&m_iFreeCount);
	}

private:
	__forceinline Node *PlacementNewAlloc() noexcept
	{
		Node *mallocNode = (Node *)malloc(sizeof(Node));
		Node *newNode = new (mallocNode) Node;
#ifdef SAFE_MODE
		newNode->poolPtr = this;
#endif
		InterlockedIncrement(&m_iCapacity);
		return newNode;
	}

	__forceinline Node *NewAlloc() noexcept
	{
		Node *newNode = new Node;
#ifdef SAFE_MODE
		newNode->poolPtr = this;
#endif
		InterlockedIncrement(&m_iCapacity);
		return newNode;
	}

private:
	bool		m_bPlacementNewFlag;
	LONG		m_iCapacity = 0;
	LONG		m_iFreeCount = 0;
	ULONG_PTR	m_ullCurrentIdentifier = 0;
	ULONG_PTR	m_pTop = 0;
};