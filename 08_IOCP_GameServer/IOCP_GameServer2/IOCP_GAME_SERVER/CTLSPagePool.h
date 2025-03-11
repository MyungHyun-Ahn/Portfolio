#pragma once

// WSASend, WSARecv ������ �� ����ȭ ����
// Byte ����� ���� Bucket�� ũ�Ⱑ ������
// ex 4KB -> BucketSize = 64 * 1024 / 4 * 1024 = 16
template<int sizeByte = 4 * 1024>
struct TLSPagePoolNode
{
	TLSPagePoolNode() = default;

	void *ptr;
	TLSPagePoolNode *next;
};

template<int sizeByte = 4 * 1024, int bucketCount = 2>
struct PageBucket
{
	static_assert((64 * 1024) % sizeByte == 0);
	// 64 ���� ū ���� ������ ������ ����
	static_assert(sizeByte >= 64);

	// ��Ŷ ������ ���
	static constexpr int BUCKET_SIZE = (64 * 1024) / sizeByte;
	static constexpr int TLS_BUCKET_COUNT = bucketCount;


	~PageBucket() noexcept
	{

	}

	static PageBucket *GetTLSPageBucket() noexcept
	{
		thread_local PageBucket buckets[TLS_BUCKET_COUNT];
		PageBucket *pPtr = buckets;
		return pPtr;
	}

	static PageBucket *GetFreePageBucket() noexcept
	{
		thread_local PageBucket freeBucket;
		return &freeBucket;
	}

	void Clear() noexcept
	{
		m_pTop = nullptr;
		m_iSize = 0;
	}

	int Push(TLSPagePoolNode<sizeByte> *freeNode) noexcept
	{
		freeNode->next = m_pTop;
		m_pTop = freeNode;
		m_iSize++;
		return m_iSize;
	}

	TLSPagePoolNode<sizeByte> *Pop() noexcept
	{
		if (m_iSize == 0)
			return nullptr;

		TLSPagePoolNode<sizeByte> *ret = m_pTop;
		m_pTop = m_pTop->next;
		m_iSize--;
		return ret;
	}

	TLSPagePoolNode<sizeByte> *m_pTop = nullptr;
	int m_iSize = 0;
};

template<int sizeByte = 4 * 1024, int bucketCount = 2, int pageLock = false>
class CTLSPagePoolManager;

template<int sizeByte = 4 * 1024, int bucketCount = 2, bool pageLock = false>
class CTLSSharedPagePool;


template<int sizeByte = 4 * 1024, int bucketCount = 2, bool pageLock = false>
class CTLSPagePool
{
public:
	friend class CTLSPagePoolManager<sizeByte, bucketCount, pageLock>;

private:
	CTLSPagePool(CTLSSharedPagePool<sizeByte, bucketCount, pageLock> *sharedPool) noexcept : m_pTLSSharedPagePool(sharedPool)
	{
		m_MyBucket = PageBucket<sizeByte, bucketCount>::GetTLSPageBucket();
		m_FreeBucket = PageBucket<sizeByte, bucketCount>::GetFreePageBucket();
	}

	~CTLSPagePool() noexcept
	{

	}

	void *Alloc() noexcept
	{
		// ���� �� ��Ȳ
		if (m_iCurrentBucketIndex == PageBucket<sizeByte, bucketCount>::TLS_BUCKET_COUNT)
		{
			m_iCurrentBucketIndex--;
		}
		else if (m_iCurrentBucketIndex != -1 && m_MyBucket[m_iCurrentBucketIndex].m_iSize == 0)
		{
			m_iCurrentBucketIndex--;
		}

		if (m_iCurrentBucketIndex == -1)
		{
			m_iCurrentBucketIndex++;
			m_MyBucket[m_iCurrentBucketIndex].m_pTop = m_pTLSSharedPagePool->Alloc();
			m_MyBucket[m_iCurrentBucketIndex].m_iSize = PageBucket<sizeByte, bucketCount>::BUCKET_SIZE;
		}

		TLSPagePoolNode<sizeByte> *retNode = m_MyBucket[m_iCurrentBucketIndex].Pop();
		void *ret = retNode->ptr;
		m_pTLSSharedPagePool->s_NodePool.Free(retNode);
		return ret;
	}

	void Free(void *delPtr) noexcept
	{
		TLSPagePoolNode<sizeByte> *delNode = m_pTLSSharedPagePool->s_NodePool.Alloc();
		delNode->ptr = delPtr;
		delNode->next = nullptr;

		if (m_iCurrentBucketIndex == PageBucket<sizeByte, bucketCount>::TLS_BUCKET_COUNT)
		{
			// Free ��Ŷ����
			int retSize = m_FreeBucket->Push(delNode);
			if (retSize == PageBucket<sizeByte, bucketCount>::BUCKET_SIZE)
			{
				// ���� Ǯ�� ��ȯ
				m_pTLSSharedPagePool->Free(m_FreeBucket->m_pTop);
				m_FreeBucket->Clear();
			}
		}
		else
		{
			if (m_iCurrentBucketIndex != -1)
			{
				int retSize = m_MyBucket[m_iCurrentBucketIndex].Push(delNode);
				if (retSize == PageBucket<sizeByte, bucketCount>::BUCKET_SIZE)
				{
					m_iCurrentBucketIndex++;
				}
			}
			else
			{
				m_iCurrentBucketIndex++;
				m_MyBucket[m_iCurrentBucketIndex].Push(delNode);
			}
		}
	}

public:
	static constexpr int MyBucketCount = 2;

private:

	PageBucket<sizeByte, bucketCount> *m_MyBucket;
	int m_iCurrentBucketIndex = -1;

	// ���� MyBucket�� ���� �� ��� -> FreeBucket�� ��ȯ
	// FreeBucket�� ���� �� ��� ���� Bucket Ǯ�� ��ȯ
	PageBucket<sizeByte, bucketCount> *m_FreeBucket;

	LONG	m_lUsedCount = 0;

	CTLSSharedPagePool<sizeByte, bucketCount, pageLock> *m_pTLSSharedPagePool = nullptr;
};

template<int sizeByte, int bucketCount, int pageLock>
class CTLSPagePoolManager
{
public:
	friend class CTLSPagePool<sizeByte, bucketCount, pageLock>;

	CTLSPagePoolManager() noexcept
	{
		m_dwTLSPagePoolIdx = TlsAlloc();
		if (m_dwTLSPagePoolIdx == TLS_OUT_OF_INDEXES)
		{
			DWORD err = GetLastError();
			__debugbreak();
		}
	}

	~CTLSPagePoolManager() noexcept
	{
		// ���� �۾�
	}

	LONG AllocTLSPagePoolIdx() noexcept
	{
		LONG idx = InterlockedIncrement(&m_iTLSPagePoolsCurrentSize);
		return idx;
	}

	void *Alloc() noexcept
	{
		__int64 tlsIndex = (__int64)TlsGetValue(m_dwTLSPagePoolIdx);
		if (tlsIndex == 0)
		{
			tlsIndex = AllocTLSPagePoolIdx();
			TlsSetValue(m_dwTLSPagePoolIdx, (LPVOID)tlsIndex);
			m_arrTLSPagePools[tlsIndex] = new CTLSPagePool<sizeByte, bucketCount, pageLock>(&m_TLSSharedPagePool);
		}
		void *ptr = m_arrTLSPagePools[tlsIndex]->Alloc();
		return ptr;
	}

	void Free(void *freePtr) noexcept
	{
		__int64 tlsIndex = (__int64)TlsGetValue(m_dwTLSPagePoolIdx);
		if (tlsIndex == 0)
		{
			tlsIndex = AllocTLSPagePoolIdx();
			TlsSetValue(m_dwTLSPagePoolIdx, (LPVOID)tlsIndex);
			m_arrTLSPagePools[tlsIndex] = new CTLSPagePool<sizeByte, bucketCount, pageLock>(&m_TLSSharedPagePool);
		}

		m_arrTLSPagePools[tlsIndex]->Free(freePtr);
	}

	LONG GetCapacity() noexcept
	{
		return m_TLSSharedPagePool.GetCapacity();
	}

	LONG GetUseCount() const noexcept
	{

	}

private:
	static constexpr int m_iThreadCount = 50;

	DWORD			m_dwTLSPagePoolIdx;

	// �ε��� 0���� TLS�� �ʱⰪ���� ����� �Ұ���
	CTLSPagePool<sizeByte, bucketCount, pageLock> *m_arrTLSPagePools[m_iThreadCount];
	LONG					m_iTLSPagePoolsCurrentSize = 0;

	CTLSSharedPagePool<sizeByte, bucketCount, pageLock> m_TLSSharedPagePool = CTLSSharedPagePool<sizeByte, bucketCount, pageLock>();
};