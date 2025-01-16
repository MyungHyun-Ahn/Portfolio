#pragma once
template<int sizeByte = 4 * 1024, int bucketCount = 2>
class CTLSSharedPagePool
{
public:
	friend class CTLSPagePool<sizeByte, bucketCount>;

	using BucketNode = TLSMemoryPoolNode<PageBucket<sizeByte, bucketCount>>;
	using Node = TLSPagePoolNode<sizeByte>;

	CTLSSharedPagePool() noexcept
	{
		m_NULL = InterlockedIncrement(&s_iQueueIdentifier) % 0xFFFF;
		ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
		BucketNode *pHead = s_BucketPool.Alloc();
		pHead->next = (BucketNode *)m_NULL;
		m_pHead = CombineIdentAndAddr(ident, (ULONG_PTR)pHead);
		m_pTail = m_pHead;
	}

	Node *Alloc() noexcept
	{
		// Free 상태인 Bucket이 있는지 먼저 체크
		if (InterlockedDecrement(&m_iUseCount) < 0)
		{
			InterlockedIncrement(&m_iUseCount);

			// 없으면 찐 할당
			Node *retTop = CreateNodeList();
			InterlockedAdd(&m_iCapacity, 64); // 64KB 단위로 집계
			return retTop;
		}

		Node *retTop;

		while (true)
		{
			ULONG_PTR readHead = m_pHead;
			ULONG_PTR readTail = m_pTail;
			BucketNode *readHeadAddr = (BucketNode *)GetAddress(readHead);
			ULONG_PTR next = (ULONG_PTR)readHeadAddr->next;
			BucketNode *nextAddr = (BucketNode *)GetAddress(next);

			// Head와 Tail이 같다면 한번 밀어주고 Dequeue 수행
			if (readHead == readTail)
			{
				BucketNode *readTailAddr = (BucketNode *)GetAddress(readTail);
				ULONG_PTR readTailNext = (ULONG_PTR)readTailAddr->next;
				InterlockedCompareExchange(&m_pTail, readTailNext, readTail);
			}

			// Head->next NULL인 경우는 큐가 비었을 때 뿐
			if (next == m_NULL)
			{
				continue;
			}
			else
			{

				if (next < 0xFFFF)
				{
					continue;
				}

				retTop = nextAddr->data.m_pTop;
				if (InterlockedCompareExchange(&m_pHead, next, readHead) == readHead)
				{
					BucketNode *readHeadAddr = (BucketNode *)GetAddress(readHead);
					s_BucketPool.Free(readHeadAddr);
					break;
				}
			}
		}

		return retTop;
	}

	// Enqueue
	void Free(TLSPagePoolNode<sizeByte> *freePtr) noexcept
	{
		// 반납
		ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);

		BucketNode *newNode = s_BucketPool.Alloc();
		newNode->data.m_pTop = freePtr;
		newNode->data.m_iSize = PageBucket<sizeByte, bucketCount>::BUCKET_SIZE;
		newNode->next = (BucketNode *)m_NULL;

		ULONG_PTR combinedNode = CombineIdentAndAddr(ident, (ULONG_PTR)newNode);

		while (true)
		{
			ULONG_PTR readTail = m_pTail;
			Node *readTailAddr = (Node *)GetAddress(readTail);
			ULONG_PTR next = (ULONG_PTR)readTailAddr->next;

			if (next != m_NULL)
			{
				InterlockedCompareExchange(&m_pTail, next, readTail);
				continue;
			}

			if (InterlockedCompareExchange((ULONG_PTR *) & readTailAddr->next, combinedNode, m_NULL) == m_NULL)
			{
				InterlockedCompareExchange(&m_pTail, combinedNode, readTail);
				break;
			}
		}

		InterlockedIncrement(&m_iUseCount);
	}

public:
	LONG GetCapacity() const noexcept
	{
		return m_iCapacity;
	}

private:
	// 버킷 개수만큼 VirtualAlloc 할당해서 나눠주기
	Node *CreateNodeList() noexcept
	{
		// 64KB 할당 단위
		BYTE *ptr = (BYTE *)VirtualAlloc(nullptr, 64 * 1024, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

		Node *pTop = nullptr;
		for (int i = 0; i < PageBucket<sizeByte, bucketCount>::BUCKET_SIZE; i++)
		{
			Node *node = s_NodePool.Alloc();
			node->next = pTop;
			node->ptr = (BYTE *)ptr + i * sizeByte;
			pTop = node;
		}

		return pTop;
	}

private:
	ULONG_PTR			m_pHead = NULL;
	ULONG_PTR			m_pTail = NULL;
	ULONG_PTR			m_ullCurrentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
	LONG				m_iUseCount = 0;
	LONG				m_iCapacity = 0;
	ULONG_PTR			m_NULL; // NULL 체크용

	inline static LONG	s_iQueueIdentifier = 0;
	inline static CTLSMemoryPoolManager<BucketNode, 64, 2> s_BucketPool = CTLSMemoryPoolManager<BucketNode, 64, 2>();
	inline static CTLSMemoryPoolManager<Node, 64, 2> s_NodePool = CTLSMemoryPoolManager<Node, 64, 2>();
};

