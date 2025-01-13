#pragma once

// KB ����� ���� Bucket�� ũ�Ⱑ ������
// ex 4KB -> BucketSize = 64 / 4 = 16
template<int sizeKB = 4>
struct TLSPagePoolNode
{
	TLSPagePoolNode() = default;

	void *ptr;
	TLSPagePoolNode *next;
};

template<int sizeKB = 4, int bucketCount = 2>
struct Bucket
{
	// 64�� ������ �������� ������ ����
	static_assert(64 % sizeKB != 0);
	// 64 ���� ū ���� ������ ������ ����
	static_assert(sizeKB > 64);

	// ��Ŷ ������ ���
	static constexpr int BUCKET_SIZE = 64 / sizeKB;
	static constexpr int TLS_BUCKET_COUNT = bucketCount;


	~Bucket() noexcept
	{

	}

	static Bucket *GetTLSBucket() noexcept
	{
		thread_local Bucket buckets[TLS_BUCKET_COUNT];
		Bucket *pPtr = buckets;
		return pPtr;
	}

	static Bucket *GetFreeBucket() noexcept
	{
		thread_local Bucket freeBucket;
		return &freeBucket;
	}

	void Clear() noexcept
	{
		m_pTop = nullptr;
		m_iSize = nullptr;
	}

	int Push(TLSPagePoolNode<sizeKB> *freeNode) noexcept
	{
		freeNode->next = m_pTop;
		m_pTop = freeNode;
		m_iSize++;
		return m_iSize;
	}

	TLSPagePoolNode<sizeKB> *Pop() noexcept
	{
		if (m_iSize == 0)
			return nullptr;

		TLSPagePoolNode<sizeKB> *ret = m_pTop;
		m_pTop = m_pTop->next;
		m_iSize--;
		return ret;
	}

	TLSPagePoolNode<sizeKB> *m_pTop = nullptr;
	int m_iSize = 0;
};

template<LONG sizeKB = 4, int bucketSize = 2>
class CTLSSharedPagePool
{
public:
	using Node = MemoryPoolNode<Bucket<sizeKB, bucketSize>>;


private:
	ULONG_PTR			m_pHead = NULL;
	ULONG_PTR			m_pTail = NULL;
	ULONG_PTR			m_ullCurrentIdentifier = 0; // ABA ������ �ذ��ϱ� ���� �ĺ���
	LONG				m_iUseCount = 0;
	LONG				m_iCapacity = 0;
	ULONG_PTR			m_NULL; // NULL üũ��

	inline static LONG	s_iQueueIdentifier = 0;
	inline static CTLSMemoryPoolManager<Node> s_BucketPool = CTLSMemoryPoolManager<Node>();
	inline static CTLSMemoryPoolManager<TLSPagePoolNode<sizeKB>> s_NodePool = CTLSMemoryPoolManager<TLSPagePoolNode<sizeKB>>();
};

