#pragma once

template<typename DATA>
struct TLSMemoryPoolNode
{
	TLSMemoryPoolNode() = default;

	DATA data;
	TLSMemoryPoolNode<DATA> *next;
};

template<typename DATA>
struct Bucket
{
	static constexpr int BUCKET_SIZE = 64;
	static constexpr int TLS_BUCKET_COUNT = 2;

	~Bucket()
	{
		
	}

	static Bucket *GetTLSBucket()
	{
		thread_local Bucket buckets[TLS_BUCKET_COUNT];
		Bucket *pPtr = buckets;
		return pPtr;
	}

	static Bucket *GetFreeBucket()
	{
		thread_local Bucket freeBucket;
		return &freeBucket;
	}



	void Clear() noexcept
	{
		m_pTop = nullptr;
		m_iSize = 0;
	}

	int Push(TLSMemoryPoolNode<DATA> *freeNode) noexcept
	{
		// Top 갱신
		freeNode->next = m_pTop;
		m_pTop = freeNode;
		m_iSize++;
		return m_iSize;
	}

	TLSMemoryPoolNode<DATA> *Pop() noexcept
	{
		if (m_iSize == 0)
			return nullptr;

		TLSMemoryPoolNode<DATA> *ret = m_pTop;
		m_pTop = m_pTop->next;
		m_iSize--;
		return ret;
	}

	// 사실상 스택 역할
	TLSMemoryPoolNode<DATA> *m_pTop = nullptr;
	int m_iSize = 0;
};

// 버킷 풀, 사실 상 스택
// Capacity 를 공용 풀에서만 측정해도 될 듯
template<typename DATA, bool useQueue = FALSE>
class CTLSSharedMemoryPool
{
public:
	using Node = MemoryPoolNode<Bucket<DATA>>;

	// BUCKET_SIZE 만큼 연결된 버킷의 m_pTop을 반환
	TLSMemoryPoolNode<DATA> *Alloc() noexcept
	{
		// Free 상태인 Bucket이 있는지 먼저 체크
		if (InterlockedDecrement(&m_iUseCount) < 0)
		{
			InterlockedIncrement(&m_iUseCount);

			// 없으면 찐 할당
			TLSMemoryPoolNode<DATA> *retTop = CreateNodeList();
			return retTop;

		}

		ULONG_PTR readTop;
		ULONG_PTR newTop;
		Node *readTopAddr;

		do 
		{
			readTop = m_pTop;
			readTopAddr = (Node *)GetAddress(readTop);
			newTop = readTopAddr->next;
		} while (InterlockedCompareExchange(&m_pTop, newTop, readTop) != readTop);
		
		// Pop 성공
		TLSMemoryPoolNode<DATA> *retTop = readTopAddr->data.m_pTop;
		readTopAddr->data.Clear();
		s_BucketPool.Free(readTopAddr);
		return retTop;
	}

	// BUCKET_SIZE 만큼 연결된 버킷의 m_pTop을 반납
	void Free(TLSMemoryPoolNode<DATA> *freePtr) noexcept
	{
		// 반납
		ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
		ULONG_PTR readTop;
		Node *newTop = s_BucketPool.Alloc();
		newTop->data.m_pTop = freePtr;
		newTop->data.m_iSize = Bucket<DATA>::BUCKET_SIZE;
		ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)newTop);

		do
		{
			// readTop 또한 ident와 합성된 주소
			readTop = m_pTop;
			newTop->next = readTop;
		} while (InterlockedCompareExchange(&m_pTop, combinedNewTop, readTop) != readTop);


		InterlockedIncrement(&m_iUseCount);
	}

private:
	// 버킷 크기만큼의 노드 리스트를 생성(스택)
	TLSMemoryPoolNode<DATA> *CreateNodeList()
	{
		TLSMemoryPoolNode<DATA> *pTop = nullptr;
		for (int i = 0; i < Bucket<DATA>::BUCKET_SIZE; i++)
		{
			TLSMemoryPoolNode<DATA> *newNode = new TLSMemoryPoolNode<DATA>();
			newNode->next = pTop;
			pTop = newNode;
		}
		return pTop;
	}

private:
	ULONG_PTR		m_pTop = NULL; // Bucket Top
	ULONG_PTR		m_ullCurrentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
	LONG			m_iUseCount = 0;
	inline static CLFMemoryPool<Node> s_BucketPool = CLFMemoryPool<Node>(100, false);
};

