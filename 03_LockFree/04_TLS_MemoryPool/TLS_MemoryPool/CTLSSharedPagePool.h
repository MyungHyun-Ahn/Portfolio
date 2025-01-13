#pragma once

// KB 사이즈에 의해 Bucket의 크기가 결정됨
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
	// 64로 나누어 떨어지면 컴파일 실패
	static_assert(64 % sizeKB != 0);
	// 64 보다 큰 값이 들어오면 컴파일 실패
	static_assert(sizeKB > 64);

	// 버킷 사이즈 계산
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
	ULONG_PTR			m_ullCurrentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
	LONG				m_iUseCount = 0;
	LONG				m_iCapacity = 0;
	ULONG_PTR			m_NULL; // NULL 체크용

	inline static LONG	s_iQueueIdentifier = 0;
	inline static CTLSMemoryPoolManager<Node> s_BucketPool = CTLSMemoryPoolManager<Node>();
	inline static CTLSMemoryPoolManager<TLSPagePoolNode<sizeKB>> s_NodePool = CTLSMemoryPoolManager<TLSPagePoolNode<sizeKB>>();
};

