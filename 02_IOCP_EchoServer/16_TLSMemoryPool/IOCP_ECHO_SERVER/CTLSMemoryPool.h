#pragma once
template<typename DATA, bool UseQueue = FALSE>
class CTLSMemoryPoolManager;

template<typename DATA, bool UseQueue = TRUE>
class CTLSMemoryPool
{
public:
	friend class CTLSMemoryPoolManager<DATA, UseQueue>;

private:
	CTLSMemoryPool(CTLSSharedMemoryPool<DATA, UseQueue> *sharedPool) : m_pTLSSharedMemoryPool(sharedPool)
	{
		m_MyBucket = Bucket<DATA>::GetTLSBucket();
		m_FreeBucket = Bucket<DATA>::GetFreeBucket();
	}

	~CTLSMemoryPool()
	{

	}

	DATA *Alloc() noexcept
	{
		// 가득 찬 상황
		if (m_iCurrentBucketIndex == Bucket<DATA>::TLS_BUCKET_COUNT)
		{
			m_iCurrentBucketIndex--;
		}
		else if (m_iCurrentBucketIndex != -1 && m_MyBucket[m_iCurrentBucketIndex].m_iSize == 0)
		{
			m_iCurrentBucketIndex--;
		}

		if (m_iCurrentBucketIndex == -1)
		{
			m_MyBucket[0].m_pTop = m_pTLSSharedMemoryPool->Alloc();
			m_MyBucket[0].m_iSize = Bucket<DATA>::BUCKET_SIZE;
			m_iCurrentBucketIndex++;
		}

		TLSMemoryPoolNode<DATA> *retNode = m_MyBucket[m_iCurrentBucketIndex].Pop();

		m_lUsedCount++;

		return &retNode->data;
	}

	void Free(DATA *delPtr) noexcept
	{
		if (m_iCurrentBucketIndex == Bucket<DATA>::TLS_BUCKET_COUNT)
		{
			// Free 버킷으로
			int retSize = m_FreeBucket->Push((TLSMemoryPoolNode<DATA> *)delPtr);
			if (retSize == Bucket<DATA>::BUCKET_SIZE)
			{
				// 공용 풀로 반환
				m_pTLSSharedMemoryPool->Free(m_FreeBucket->m_pTop);
				m_FreeBucket->Clear();
			}
		}
		else
		{
			if (m_iCurrentBucketIndex != -1)
			{
				int retSize = m_MyBucket[m_iCurrentBucketIndex].Push((TLSMemoryPoolNode<DATA> *)delPtr);
				if (retSize == Bucket<DATA>::BUCKET_SIZE)
				{
					m_iCurrentBucketIndex++;
				}
			}
			else
			{
				m_MyBucket[0].m_pTop = (TLSMemoryPoolNode<DATA> *)delPtr;
				m_MyBucket[0].m_iSize++;
				m_iCurrentBucketIndex++;
			}
		}

		m_lUsedCount--;

	}

public:
	static constexpr int MyBucketCount = 2;

private:

	Bucket<DATA> *m_MyBucket;
	int m_iCurrentBucketIndex = -1;

	// 만약 MyBucket이 가득 찬 경우 -> FreeBucket에 반환
	// FreeBucket도 가득 찬 경우 공용 Bucket 풀에 반환
	Bucket<DATA> *m_FreeBucket;

	LONG	m_lUsedCount = 0;

	CTLSSharedMemoryPool<DATA, UseQueue> *m_pTLSSharedMemoryPool = nullptr;
};

template<typename DATA, bool UseQueue>
class CTLSMemoryPoolManager
{
public:
	friend class CTLSMemoryPool<DATA, UseQueue>;

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
			m_arrTLSMemoryPools[tlsIndex] = new CTLSMemoryPool<DATA, UseQueue>(&m_TLSSharedMemoryPool);
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
			m_arrTLSMemoryPools[tlsIndex] = new CTLSMemoryPool<DATA, UseQueue>(&m_TLSSharedMemoryPool);
		}

		m_arrTLSMemoryPools[tlsIndex]->Free(freePtr);
	}

	LONG GetCapacity()
	{
		return m_TLSSharedMemoryPool.GetCapacity();
	}

	LONG GetUseCount()
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
	CTLSMemoryPool<DATA, UseQueue> *m_arrTLSMemoryPools[m_iThreadCount];
	LONG					m_iTLSMemoryPoolsCurrentSize = 0;

	CTLSSharedMemoryPool<DATA, UseQueue> m_TLSSharedMemoryPool = CTLSSharedMemoryPool<DATA, UseQueue>();
};