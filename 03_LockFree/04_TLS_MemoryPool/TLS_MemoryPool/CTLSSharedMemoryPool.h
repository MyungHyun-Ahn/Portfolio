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
		// Top ����
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

	// ��ǻ� ���� ����
	TLSMemoryPoolNode<DATA> *m_pTop = nullptr;
	int m_iSize = 0;
};

// ��Ŷ Ǯ, ��� �� ����
// Capacity �� ���� Ǯ������ �����ص� �� ��
template<typename DATA, bool useQueue = FALSE>
class CTLSSharedMemoryPool
{
public:
	using Node = MemoryPoolNode<Bucket<DATA>>;

	// BUCKET_SIZE ��ŭ ����� ��Ŷ�� m_pTop�� ��ȯ
	TLSMemoryPoolNode<DATA> *Alloc() noexcept
	{
		// Free ������ Bucket�� �ִ��� ���� üũ
		if (InterlockedDecrement(&m_iUseCount) < 0)
		{
			InterlockedIncrement(&m_iUseCount);

			// ������ �� �Ҵ�
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
		
		// Pop ����
		TLSMemoryPoolNode<DATA> *retTop = readTopAddr->data.m_pTop;
		readTopAddr->data.Clear();
		s_BucketPool.Free(readTopAddr);
		return retTop;
	}

	// BUCKET_SIZE ��ŭ ����� ��Ŷ�� m_pTop�� �ݳ�
	void Free(TLSMemoryPoolNode<DATA> *freePtr) noexcept
	{
		// �ݳ�
		ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
		ULONG_PTR readTop;
		Node *newTop = s_BucketPool.Alloc();
		newTop->data.m_pTop = freePtr;
		newTop->data.m_iSize = Bucket<DATA>::BUCKET_SIZE;
		ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)newTop);

		do
		{
			// readTop ���� ident�� �ռ��� �ּ�
			readTop = m_pTop;
			newTop->next = readTop;
		} while (InterlockedCompareExchange(&m_pTop, combinedNewTop, readTop) != readTop);


		InterlockedIncrement(&m_iUseCount);
	}

private:
	// ��Ŷ ũ�⸸ŭ�� ��� ����Ʈ�� ����(����)
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
	ULONG_PTR		m_ullCurrentIdentifier = 0; // ABA ������ �ذ��ϱ� ���� �ĺ���
	LONG			m_iUseCount = 0;
	inline static CLFMemoryPool<Node> s_BucketPool = CLFMemoryPool<Node>(100, false);
};

