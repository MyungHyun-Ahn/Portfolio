#pragma once
template<typename DATA, int bucketSize = 64, int bucketCount = 2, bool UseQueue = TRUE>
class CTLSMemoryPoolManager;

template<typename DATA, int bucketSize = 64, int bucketCount = 2, bool UseQueue = TRUE>
class CTLSMemoryPool
{
public:
	friend class CTLSMemoryPoolManager<DATA, bucketSize, bucketCount, UseQueue>;

private:
	CTLSMemoryPool(CTLSSharedMemoryPool<DATA, bucketSize, bucketCount, UseQueue> *sharedPool) noexcept : m_pTLSSharedMemoryPool(sharedPool)
	{
		m_MyBucket = Bucket<DATA, bucketSize, bucketCount>::GetTLSBucket();
		m_FreeBucket = Bucket<DATA, bucketSize, bucketCount>::GetFreeBucket();
	}

	~CTLSMemoryPool() noexcept
	{

	}

	DATA *Alloc() noexcept
	{
		// 가득 찬 상황
		if (m_iCurrentBucketIndex == Bucket<DATA, bucketSize, bucketCount>::TLS_BUCKET_COUNT)
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
			m_MyBucket[m_iCurrentBucketIndex].m_pTop = m_pTLSSharedMemoryPool->Alloc();
			m_MyBucket[m_iCurrentBucketIndex].m_iSize = Bucket<DATA, bucketSize, bucketCount>::BUCKET_SIZE;
		}

		TLSMemoryPoolNode<DATA> *retNode = m_MyBucket[m_iCurrentBucketIndex].Pop();

		m_lUsedCount++;

		return &retNode->data;
	}

	void Free(DATA *delPtr) noexcept
	{
		if (m_iCurrentBucketIndex == Bucket<DATA, bucketSize, bucketCount>::TLS_BUCKET_COUNT)
		{
			// Free 버킷으로
			int retSize = m_FreeBucket->Push((TLSMemoryPoolNode<DATA> *)delPtr);
			if (retSize == Bucket<DATA, bucketSize, bucketCount>::BUCKET_SIZE)
			{
				// 공용 풀로 반환
				TLSMemoryPoolNode<DATA> *node = m_FreeBucket->m_pTop;
				m_pTLSSharedMemoryPool->Free(m_FreeBucket->m_pTop);
				m_FreeBucket->Clear();
			}
		}
		else
		{
			if (m_iCurrentBucketIndex != -1)
			{
				int retSize = m_MyBucket[m_iCurrentBucketIndex].Push((TLSMemoryPoolNode<DATA> *)delPtr);
				if (retSize == Bucket<DATA, bucketSize, bucketCount>::BUCKET_SIZE)
				{
					m_iCurrentBucketIndex++;
				}
			}
			else
			{
				m_iCurrentBucketIndex++;
				m_MyBucket[m_iCurrentBucketIndex].Push((TLSMemoryPoolNode<DATA> *)delPtr);
			}
		}

		m_lUsedCount--;
	}

private:

	Bucket<DATA, bucketSize, bucketCount> *m_MyBucket;
	int m_iCurrentBucketIndex = -1;

	// 만약 MyBucket이 가득 찬 경우 -> FreeBucket에 반환
	// FreeBucket도 가득 찬 경우 공용 Bucket 풀에 반환
	Bucket<DATA, bucketSize, bucketCount> *m_FreeBucket;

	LONG	m_lUsedCount = 0;

	CTLSSharedMemoryPool<DATA, bucketSize, bucketCount, UseQueue> *m_pTLSSharedMemoryPool = nullptr;
};

template<typename DATA, int bucketSize, int bucketCount, bool UseQueue>
class CTLSMemoryPoolManager
{
public:
	friend class CTLSMemoryPool<DATA, bucketSize, bucketCount, UseQueue>;

	CTLSMemoryPoolManager() noexcept
	{
		m_dwTLSMemoryPoolIdx = TlsAlloc();
		if (m_dwTLSMemoryPoolIdx == TLS_OUT_OF_INDEXES)
		{
			DWORD err = GetLastError();
			__debugbreak();
		}
	}

	~CTLSMemoryPoolManager() noexcept
	{
		// TLS 풀 정리 작업
	}

	LONG AllocTLSMemoryPoolIdx() noexcept
	{
		LONG idx = InterlockedIncrement(&m_iTLSMemoryPoolsCurrentSize);
		return idx;
	}

	DATA *Alloc() noexcept
	{
		__int64 tlsIndex = (__int64)TlsGetValue(m_dwTLSMemoryPoolIdx);
		if (tlsIndex == 0)
		{
			tlsIndex = AllocTLSMemoryPoolIdx();
			TlsSetValue(m_dwTLSMemoryPoolIdx, (LPVOID)tlsIndex);
			m_arrTLSMemoryPools[tlsIndex] = new CTLSMemoryPool<DATA, bucketSize, bucketCount, UseQueue>(&m_TLSSharedMemoryPool);
		}

		DATA *ptr = m_arrTLSMemoryPools[tlsIndex]->Alloc();
		return ptr;
	}

	void Free(DATA *freePtr) noexcept
	{
		__int64 tlsIndex = (__int64)TlsGetValue(m_dwTLSMemoryPoolIdx);
		if (tlsIndex == 0)
		{
			tlsIndex = AllocTLSMemoryPoolIdx();
			TlsSetValue(m_dwTLSMemoryPoolIdx, (LPVOID)tlsIndex);
			m_arrTLSMemoryPools[tlsIndex] = new CTLSMemoryPool<DATA, bucketSize, bucketCount, UseQueue>(&m_TLSSharedMemoryPool);
		}

		m_arrTLSMemoryPools[tlsIndex]->Free(freePtr);
	}

	LONG GetCapacity() noexcept
	{
		return m_TLSSharedMemoryPool.GetCapacity();
	}

	LONG GetUseCount() const noexcept
	{
		LONG usedSize = 0;
		for (int i = 1; i <= m_iTLSMemoryPoolsCurrentSize; i++)
		{
			usedSize += m_arrTLSMemoryPools[i]->m_lUsedCount;
		}

		return usedSize;
	}


private:
	static constexpr int m_iThreadCount = 50;

	DWORD			m_dwTLSMemoryPoolIdx;

	// 인덱스 0번은 TLS의 초기값으로 사용이 불가능
	CTLSMemoryPool<DATA, bucketSize, bucketCount, UseQueue> *m_arrTLSMemoryPools[m_iThreadCount];
	LONG					m_iTLSMemoryPoolsCurrentSize = 0;

	CTLSSharedMemoryPool<DATA, bucketSize, bucketCount, UseQueue> m_TLSSharedMemoryPool = CTLSSharedMemoryPool<DATA, bucketSize, bucketCount, UseQueue>();
};