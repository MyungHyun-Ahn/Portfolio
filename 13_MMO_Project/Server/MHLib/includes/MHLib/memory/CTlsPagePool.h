#pragma once

namespace MHLib::memory
{
	using namespace MHLib::containers;

	// WSASend, WSARecv 페이지 락 최적화 목적
	// Byte 사이즈에 의해 Bucket의 크기가 결정됨
	// ex 4KB -> BucketSize = 64 * 1024 / 4 * 1024 = 16
	template<int sizeByte = 4 * 1024>
	struct STlsPagePoolNode
	{
		STlsPagePoolNode() = default;

		void *m_pObject;
		STlsPagePoolNode *m_pNext;
	};

	template<int sizeByte = 4 * 1024, int bucketCount = 2>
	struct SPageBucket
	{
		static_assert((64 * 1024) % sizeByte == 0);
		// 64 보다 큰 값이 들어오면 컴파일 실패
		static_assert(sizeByte >= 64);

		// 버킷 사이즈 계산
		static constexpr int kBucketSize = (64 * 1024) / sizeByte;
		static constexpr int kTlsBucketCount = bucketCount;


		~SPageBucket() noexcept
		{

		}

		static SPageBucket *GetTlsPageBucket() noexcept
		{
			thread_local SPageBucket buckets[kTlsBucketCount];
			SPageBucket *pPtr = buckets;
			return pPtr;
		}

		static SPageBucket *GetFreePageBucket() noexcept
		{
			thread_local SPageBucket freeBucket;
			return &freeBucket;
		}

		void Clear() noexcept
		{
			m_pTop = nullptr;
			m_size = 0;
		}

		int Push(STlsPagePoolNode<sizeByte> *pFreeNode) noexcept
		{
			pFreeNode->m_pNext = m_pTop;
			m_pTop = pFreeNode;
			m_size++;
			return m_size;
		}

		STlsPagePoolNode<sizeByte> *Pop() noexcept
		{
			if (m_size == 0)
				return nullptr;

			STlsPagePoolNode<sizeByte> *pRet = m_pTop;
			m_pTop = m_pTop->m_pNext;
			m_size--;
			return pRet;
		}

		STlsPagePoolNode<sizeByte> *m_pTop = nullptr;
		int m_size = 0;
	};

	template<int sizeByte = 4 * 1024, int bucketCount = 2, int pageLock = false>
	class CTlsPagePoolManager;

	template<int sizeByte = 4 * 1024, int bucketCount = 2, bool pageLock = false>
	class CTlsSharedPagePool;


	template<int sizeByte = 4 * 1024, int bucketCount = 2, bool pageLock = false>
	class CTlsPagePool
	{
	public:
		friend class CTlsPagePoolManager<sizeByte, bucketCount, pageLock>;

	private:
		CTlsPagePool(CTlsSharedPagePool<sizeByte, bucketCount, pageLock> *pSharedPool) noexcept : m_pTlsSharedPagePool(pSharedPool)
		{
			m_pMyBucket = SPageBucket<sizeByte, bucketCount>::GetTlsPageBucket();
			m_pFreeBucket = SPageBucket<sizeByte, bucketCount>::GetFreePageBucket();
		}

		~CTlsPagePool() noexcept
		{

		}

		void *alloc() noexcept
		{
			// 가득 찬 상황
			if (m_currentBucketIndex == SPageBucket<sizeByte, bucketCount>::kTlsBucketCount)
			{
				m_currentBucketIndex--;
			}
			else if (m_currentBucketIndex != -1 && m_pMyBucket[m_currentBucketIndex].m_size == 0)
			{
				m_currentBucketIndex--;
			}

			if (m_currentBucketIndex == -1)
			{
				m_currentBucketIndex++;
				m_pMyBucket[m_currentBucketIndex].m_pTop = m_pTlsSharedPagePool->Alloc();
				m_pMyBucket[m_currentBucketIndex].m_size = SPageBucket<sizeByte, bucketCount>::kBucketSize;
			}

			STlsPagePoolNode<sizeByte> *pRetNode = m_pMyBucket[m_currentBucketIndex].Pop();
			void *pRet = pRetNode->m_pObject;
			m_pTlsSharedPagePool->sm_nodePool.Free(pRetNode);
			return pRet;
		}

		void free(void *pDelPtr) noexcept
		{
			STlsPagePoolNode<sizeByte> *pDelNode = m_pTlsSharedPagePool->sm_nodePool.Alloc();
			pDelNode->m_pObject = pDelPtr;
			pDelNode->m_pNext = nullptr;

			if (m_currentBucketIndex == SPageBucket<sizeByte, bucketCount>::kTlsBucketCount)
			{
				// Free 버킷으로
				int retSize = m_pFreeBucket->Push(pDelNode);
				if (retSize == SPageBucket<sizeByte, bucketCount>::kBucketSize)
				{
					// 공용 풀로 반환
					m_pTlsSharedPagePool->Free(m_pFreeBucket->m_pTop);
					m_pFreeBucket->Clear();
				}
			}
			else
			{
				if (m_currentBucketIndex != -1)
				{
					int retSize = m_pMyBucket[m_currentBucketIndex].Push(pDelNode);
					if (retSize == SPageBucket<sizeByte, bucketCount>::kBucketSize)
					{
						m_currentBucketIndex++;
					}
				}
				else
				{
					m_currentBucketIndex++;
					m_pMyBucket[m_currentBucketIndex].Push(pDelNode);
				}
			}
		}

	public:
		static constexpr int kMyBucketCount = 2;

	private:

		SPageBucket<sizeByte, bucketCount> *m_pMyBucket;
		int m_currentBucketIndex = -1;

		// 만약 MyBucket이 가득 찬 경우 -> FreeBucket에 반환
		// FreeBucket도 가득 찬 경우 공용 Bucket 풀에 반환
		SPageBucket<sizeByte, bucketCount> *m_pFreeBucket;

		LONG	m_usedCount = 0;

		CTlsSharedPagePool<sizeByte, bucketCount, pageLock> *m_pTlsSharedPagePool = nullptr;
	};

	template<int sizeByte, int bucketCount, int pageLock>
	class CTlsPagePoolManager
	{
	public:
		friend class CTlsPagePool<sizeByte, bucketCount, pageLock>;

		CTlsPagePoolManager() noexcept
		{
			m_tlsPagePoolIdx = TlsAlloc();
			if (m_tlsPagePoolIdx == TLS_OUT_OF_INDEXES)
			{
				DWORD err = GetLastError();
				__debugbreak();
			}
		}

		~CTlsPagePoolManager() noexcept
		{
			// 정리 작업
		}

		LONG AllocTlsPagePoolIdx() noexcept
		{
			LONG idx = InterlockedIncrement(&m_tlsPagePoolsCurrentSize);
			return idx;
		}

		void *Alloc() noexcept
		{
			__int64 tlsIndex = (__int64)TlsGetValue(m_tlsPagePoolIdx);
			if (tlsIndex == 0)
			{
				tlsIndex = AllocTlsPagePoolIdx();
				TlsSetValue(m_tlsPagePoolIdx, (LPVOID)tlsIndex);
				m_pTlsPagePools[tlsIndex] = new CTlsPagePool<sizeByte, bucketCount, pageLock>(&m_tlsSharedPagePool);
			}
			void *pPtr = m_pTlsPagePools[tlsIndex]->alloc();
			return pPtr;
		}

		void Free(void *pFreePtr) noexcept
		{
			__int64 tlsIndex = (__int64)TlsGetValue(m_tlsPagePoolIdx);
			if (tlsIndex == 0)
			{
				tlsIndex = AllocTlsPagePoolIdx();
				TlsSetValue(m_tlsPagePoolIdx, (LPVOID)tlsIndex);
				m_pTlsPagePools[tlsIndex] = new CTlsPagePool<sizeByte, bucketCount, pageLock>(&m_tlsSharedPagePool);
			}

			m_pTlsPagePools[tlsIndex]->free(pFreePtr);
		}

		LONG GetCapacity() noexcept
		{
			return m_tlsSharedPagePool.GetCapacity();
		}

	private:
		static constexpr int kThreadCount = 50;

		DWORD			m_tlsPagePoolIdx;

		// 인덱스 0번은 TLS의 초기값으로 사용이 불가능
		CTlsPagePool<sizeByte, bucketCount, pageLock> *m_pTlsPagePools[kThreadCount];
		LONG					m_tlsPagePoolsCurrentSize = 0;

		CTlsSharedPagePool<sizeByte, bucketCount, pageLock> m_tlsSharedPagePool = CTlsSharedPagePool<sizeByte, bucketCount, pageLock>();
	};

	template<int sizeByte, int bucketCount, bool pageLock>
	class CTlsSharedPagePool
	{
	public:
		friend class CTlsPagePool<sizeByte, bucketCount, pageLock>;

		using SBucketNode = STlsMemoryPoolNode<SPageBucket<sizeByte, bucketCount>>;
		using SNode = STlsPagePoolNode<sizeByte>;

		CTlsSharedPagePool() noexcept
		{
			m_null = InterlockedIncrement(&sm_queueIdentifier) % 0xFFFF;
			ULONG_PTR ident = InterlockedIncrement(&m_currentIdentifier);
			SBucketNode *pHead = sm_bucketPool.Alloc();
			pHead->m_pNext = (SBucketNode *)m_null;
			m_head = CombineIdentAndAddr(ident, (ULONG_PTR)pHead);
			m_tail = m_head;
		}

		SNode *Alloc() noexcept
		{
			// Free 상태인 Bucket이 있는지 먼저 체크
			if (InterlockedDecrement(&m_useCount) < 0)
			{
				InterlockedIncrement(&m_useCount);

				// 없으면 진짜 할당
				SNode *pRetTop = createNodeList();
				InterlockedAdd(&m_capacity, 64); // 64KB 단위로 집계
				return pRetTop;
			}

			SNode *pRetTop;

			while (true)
			{
				ULONG_PTR readHead = m_head;
				ULONG_PTR readTail = m_tail;
				SBucketNode *pReadHeadAddr = (SBucketNode *)GetAddress(readHead);
				ULONG_PTR next = (ULONG_PTR)pReadHeadAddr->m_pNext;
				SBucketNode *pNextAddr = (SBucketNode *)GetAddress(next);

				// Head와 Tail이 같다면 한번 밀어주고 Dequeue 수행
				if (readHead == readTail)
				{
					SBucketNode *pReadTailAddr = (SBucketNode *)GetAddress(readTail);
					ULONG_PTR readTailNext = (ULONG_PTR)pReadTailAddr->m_pNext;
					InterlockedCompareExchange(&m_tail, readTailNext, readTail);
				}

				// Head->next NULL인 경우는 큐가 비었을 때 뿐
				if (next == m_null)
				{
					continue;
				}
				else
				{

					if (next < 0xFFFF)
					{
						continue;
					}

					pRetTop = pNextAddr->m_data.m_pTop;
					if (InterlockedCompareExchange(&m_head, next, readHead) == readHead)
					{
						SBucketNode *pReadHeadAddr = (SBucketNode *)GetAddress(readHead);
						sm_bucketPool.Free(pReadHeadAddr);
						break;
					}
				}
			}

			return pRetTop;
		}

		// Enqueue
		void Free(STlsPagePoolNode<sizeByte> *pFreePtr) noexcept
		{
			// 반납
			ULONG_PTR ident = InterlockedIncrement(&m_currentIdentifier);

			SBucketNode *pNewNode = sm_bucketPool.Alloc();
			pNewNode->m_data.m_pTop = pFreePtr;
			pNewNode->m_data.m_size = SPageBucket<sizeByte, bucketCount>::kBucketSize;
			pNewNode->m_pNext = (SBucketNode *)m_null;

			ULONG_PTR combinedNode = CombineIdentAndAddr(ident, (ULONG_PTR)pNewNode);

			while (true)
			{
				ULONG_PTR readTail = m_tail;
				SNode *pReadTailAddr = (SNode *)GetAddress(readTail);
				ULONG_PTR next = (ULONG_PTR)pReadTailAddr->m_pNext;

				if (next != m_null)
				{
					InterlockedCompareExchange(&m_tail, next, readTail);
					continue;
				}

				if (InterlockedCompareExchange((ULONG_PTR *)&pReadTailAddr->m_pNext, combinedNode, m_null) == m_null)
				{
					InterlockedCompareExchange(&m_tail, combinedNode, readTail);
					break;
				}
			}

			InterlockedIncrement(&m_useCount);
		}

	public:
		LONG GetCapacity() const noexcept
		{
			return m_capacity;
		}

	private:
		// 버킷 개수만큼 VirtualAlloc 할당해서 나눠주기
		SNode *createNodeList() noexcept
		{
			// 64KB 할당 단위
			BYTE *pPtr = (BYTE *)VirtualAlloc(nullptr, 64 * 1024, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

			if constexpr (pageLock)
			{
				int err;
				if (VirtualLock(pPtr, 64 * 1024) == FALSE)
				{
					err = GetLastError();
					__debugbreak();
				}
			}

			SNode *pTop = nullptr;
			for (int i = 0; i < SPageBucket<sizeByte, bucketCount>::kBucketSize; i++)
			{
				SNode *pNode = sm_nodePool.Alloc();
				pNode->m_pNext = pTop;
				pNode->m_pObject = (BYTE *)pPtr + i * sizeByte;
				pTop = pNode;
			}

			return pTop;
		}

	private:
		ULONG_PTR			m_head = NULL;
		ULONG_PTR			m_tail = NULL;
		ULONG_PTR			m_currentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
		LONG				m_useCount = 0;
		LONG				m_capacity = 0;
		ULONG_PTR			m_null; // NULL 체크용

		inline static LONG	sm_queueIdentifier = 0;
		inline static CTlsMemoryPoolManager<SBucketNode, 64, 2> sm_bucketPool = CTlsMemoryPoolManager<SBucketNode, 64, 2>();
		inline static CTlsMemoryPoolManager<SNode, 64, 2> sm_nodePool = CTlsMemoryPoolManager<SNode, 64, 2>();
	};
}