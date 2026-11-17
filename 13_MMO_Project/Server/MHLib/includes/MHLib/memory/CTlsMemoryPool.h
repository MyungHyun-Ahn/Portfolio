#pragma once

namespace MHLib::memory
{
	using namespace MHLib::containers;

	template<typename TData>
	struct STlsMemoryPoolNode
	{
		STlsMemoryPoolNode() = default;

		TData m_data;
		STlsMemoryPoolNode<TData> *m_pNext;
	};

	template<typename TData, int bucketSize = 64, int bucketCount = 2>
	struct SBucket
	{
		static constexpr int kBucketSize = bucketSize;
		static constexpr int kTlsBucketCount = bucketCount;

		~SBucket() noexcept
		{
			// 누수 감지 코드 등 넣어도 됨
		}

		static SBucket *GetTlsBucket() noexcept
		{
			thread_local SBucket buckets[kTlsBucketCount];
			SBucket *pPtr = buckets;
			return pPtr;
		}

		static SBucket *GetFreeBucket() noexcept
		{
			thread_local SBucket freeBucket;
			return &freeBucket;
		}



		inline void Clear() noexcept
		{
			m_pTop = nullptr;
			m_size = 0;
		}

		inline int Push(STlsMemoryPoolNode<TData> *pFreeNode) noexcept
		{
			// Top 갱신
			pFreeNode->m_pNext = m_pTop;
			m_pTop = pFreeNode;
			m_size++;
			return m_size;
		}

		inline STlsMemoryPoolNode<TData> *Pop() noexcept
		{
			if (m_size == 0)
				return nullptr;

			STlsMemoryPoolNode<TData> *pRet = m_pTop;
			m_pTop = m_pTop->m_pNext;
			m_size--;

			return pRet;
		}

		// 사실상 스택 역할
		STlsMemoryPoolNode<TData> *m_pTop = nullptr;
		int m_size = 0;
	};

	// 기본 값은 큐 쓰도록
	// 테스트 결과 큐가 조금 더 괜찮았음
	template<typename TData, int bucketSize = 64, int bucketCount = 2, bool useQueue = TRUE>
	class CTlsSharedMemoryPool
	{

	};

	// 버킷 풀, 사실 상 스택
	// Capacity 를 공용 풀에서만 측정해도 될 듯
	template<typename TData, int bucketSize, int bucketCount>
	class CTlsSharedMemoryPool<TData, bucketSize, bucketCount, FALSE>
	{
	public:
		using SNode = SMemoryPoolNode<SBucket<TData, bucketSize, bucketCount>>;

		// BUCKET_SIZE 만큼 연결된 버킷의 m_pTop을 반환
		STlsMemoryPoolNode<TData> *Alloc() noexcept
		{
			// Free 상태인 Bucket이 있는지 먼저 체크
			if (InterlockedDecrement(&m_useCount) < 0)
			{
				InterlockedIncrement(&m_useCount);
				// 없으면 찐 할당
				STlsMemoryPoolNode<TData> *pRetTop = createNodeList();
				STlsMemoryPoolNode<TData> *pNode = pRetTop;

				InterlockedAdd(&m_capacity, SBucket<TData, bucketSize, bucketCount>::kBucketSize);
				return pRetTop;

			}

			ULONG_PTR readTop;
			ULONG_PTR newTop;
			SNode *pReadTopAddr;

			do
			{
				readTop = m_top;
				pReadTopAddr = (SNode *)GetAddress(readTop);
				newTop = pReadTopAddr->m_next;
			} while (InterlockedCompareExchange(&m_top, newTop, readTop) != readTop);

			// Pop 성공
			STlsMemoryPoolNode<TData> *pRetTop = pReadTopAddr->m_data.m_pTop;

			pReadTopAddr->m_data.Clear();
			sm_bucketPool.Free(pReadTopAddr);
			return pRetTop;
		}

		// BUCKET_SIZE 만큼 연결된 버킷의 m_pTop을 반납
		void Free(STlsMemoryPoolNode<TData> *pFreePtr) noexcept
		{
			// 반납
			ULONG_PTR ident = InterlockedIncrement(&m_currentIdentifier);
			ULONG_PTR readTop;
			SNode *pNewTop = sm_bucketPool.Alloc();
			pNewTop->m_data.m_pTop = pFreePtr;
			pNewTop->m_data.m_size = SBucket<TData, bucketSize, bucketCount>::kBucketSize;
			ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)pNewTop);

			do
			{
				// readTop 또한 ident와 합성된 주소
				readTop = m_top;
				pNewTop->m_next = readTop;
			} while (InterlockedCompareExchange(&m_top, combinedNewTop, readTop) != readTop);


			InterlockedIncrement(&m_useCount);
		}

	public:
		inline LONG GetCapacity() const noexcept
		{
			return m_capacity;
		}

	private:
		// 버킷 크기만큼의 노드 리스트를 생성(스택)
		inline STlsMemoryPoolNode<TData> *createNodeList() noexcept
		{
			STlsMemoryPoolNode<TData> *pTop = nullptr;
			for (int i = 0; i < SBucket<TData, bucketSize, bucketCount>::kBucketSize; i++)
			{
				STlsMemoryPoolNode<TData> *pNewNode = new STlsMemoryPoolNode<TData>();
				pNewNode->m_pNext = pTop;
				pTop = pNewNode;
			}
			return pTop;
		}

	private:
		ULONG_PTR		m_top = NULL; // Bucket Top
		ULONG_PTR		m_currentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
		LONG			m_useCount = 0;
		LONG			m_capacity = 0;
		inline static CLfMemoryPool<SNode> sm_bucketPool = CLfMemoryPool<SNode>(100);
	};

	template<typename TData, int bucketSize, int bucketCount>
	class CTlsSharedMemoryPool<TData, bucketSize, bucketCount, TRUE>
	{
	public:
		using SNode = SMemoryPoolNode<SBucket<TData, bucketSize, bucketCount>>;

		// 생성자
		CTlsSharedMemoryPool<TData, bucketSize, bucketCount>() noexcept
		{
			m_null = InterlockedIncrement(&sm_queueIdentifier) % 0xFFFF;
			ULONG_PTR ident = InterlockedIncrement(&m_currentIdentifier);
			SNode *pHead = sm_bucketPool.Alloc();
			pHead->m_next = m_null;
			m_head = CombineIdentAndAddr(ident, (ULONG_PTR)pHead);
			m_tail = m_head;
		}

		// Dequeue
		STlsMemoryPoolNode<TData> *Alloc() noexcept
		{
			// Free 상태인 Bucket이 있는지 먼저 체크
			if (InterlockedDecrement(&m_useCount) < 0)
			{
				InterlockedIncrement(&m_useCount);

				// 없으면 찐 할당
				STlsMemoryPoolNode<TData> *pRetTop = createNodeList();
				InterlockedAdd(&m_capacity, SBucket<TData, bucketSize, bucketCount>::kBucketSize);
				return pRetTop;

			}

			STlsMemoryPoolNode<TData> *pRetTop;

			while (true)
			{
				ULONG_PTR readHead = m_head;
				ULONG_PTR readTail = m_tail;
				SNode *pReadHeadAddr = (SNode *)GetAddress(readHead);
				ULONG_PTR next = pReadHeadAddr->m_next;
				SNode *pNextAddr = (SNode *)GetAddress(next);

				// Head와 Tail이 같다면 한번 밀어주고 Dequeue 수행
				if (readHead == readTail)
				{
					SNode *pReadTailAddr = (SNode *)GetAddress(readTail);
					ULONG_PTR readTailNext = pReadTailAddr->m_next;
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
						pReadHeadAddr = (SNode *)GetAddress(readHead);
						sm_bucketPool.Free(pReadHeadAddr);
						break;
					}
				}
			}

			return pRetTop;
		}

		// Enqueue
		void Free(STlsMemoryPoolNode<TData> *pFreePtr) noexcept
		{
			// 반납
			ULONG_PTR ident = InterlockedIncrement(&m_currentIdentifier);

			SNode *pNewNode = sm_bucketPool.Alloc();
			pNewNode->m_data.m_pTop = pFreePtr;
			pNewNode->m_data.m_size = SBucket<TData, bucketSize, bucketCount>::kBucketSize;
			pNewNode->m_next = m_null;

			ULONG_PTR combinedNode = CombineIdentAndAddr(ident, (ULONG_PTR)pNewNode);

			while (true)
			{
				ULONG_PTR readTail = m_tail;
				SNode *pReadTailAddr = (SNode *)GetAddress(readTail);
				ULONG_PTR next = pReadTailAddr->m_next;

				if (next != m_null)
				{
					InterlockedCompareExchange(&m_tail, next, readTail);
					continue;
				}

				if (InterlockedCompareExchange(&pReadTailAddr->m_next, combinedNode, m_null) == m_null)
				{
					InterlockedCompareExchange(&m_tail, combinedNode, readTail);
					break;
				}
			}

			InterlockedIncrement(&m_useCount);
		}

	public:
		inline LONG GetCapacity() const noexcept
		{
			return m_capacity;
		}

	private:
		// 버킷 크기만큼의 노드 리스트를 생성(스택)
		inline STlsMemoryPoolNode<TData> *createNodeList() noexcept
		{
			STlsMemoryPoolNode<TData> *pTop = nullptr;
			for (int i = 0; i < SBucket<TData, bucketSize, bucketCount>::kBucketSize; i++)
			{
				STlsMemoryPoolNode<TData> *pNewNode = new STlsMemoryPoolNode<TData>();
				pNewNode->m_pNext = pTop;
				pTop = pNewNode;
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
		inline static CLfMemoryPool<SNode> sm_bucketPool = CLfMemoryPool<SNode>(100);
	};

	template<typename TData, int bucketSize = 64, int bucketCount = 2, bool useQueue = TRUE>
	class CTlsMemoryPoolManager;

	template<typename TData, int bucketSize = 64, int bucketCount = 2, bool useQueue = TRUE>
	class CTlsMemoryPool
	{
	public:
		friend class CTlsMemoryPoolManager<TData, bucketSize, bucketCount, useQueue>;

	private:
		CTlsMemoryPool(CTlsSharedMemoryPool<TData, bucketSize, bucketCount, useQueue> *pSharedPool) noexcept
			: m_pTlsSharedMemoryPool(pSharedPool)
		{
			m_pAllocBucket = SBucket<TData, bucketSize, bucketCount>::GetTlsBucket();
			m_pFreeBucket = SBucket<TData, bucketSize, bucketCount>::GetFreeBucket();
		}

		~CTlsMemoryPool() noexcept
		{

		}

		TData *alloc() noexcept
		{
			if (m_currentBucketIndex == SBucket<TData, bucketSize, bucketCount>::kTlsBucketCount)
			{
				m_currentBucketIndex--;
			}
			else if (m_currentBucketIndex != -1 && m_pAllocBucket[m_currentBucketIndex].m_size == 0)
			{
				m_currentBucketIndex--;
			}

			if (m_currentBucketIndex == -1)
			{
				m_currentBucketIndex++;
				m_pAllocBucket[m_currentBucketIndex].m_pTop = m_pTlsSharedMemoryPool->Alloc();
				m_pAllocBucket[m_currentBucketIndex].m_size = SBucket<TData, bucketSize, bucketCount>::kBucketSize;
			}

			STlsMemoryPoolNode<TData> *pRetNode = m_pAllocBucket[m_currentBucketIndex].Pop();
			m_usedCount++;
			return &pRetNode->m_data;
		}

		void free(TData *pDelPtr) noexcept
		{
			if (m_currentBucketIndex == SBucket<TData, bucketSize, bucketCount>::kTlsBucketCount)
			{
				int retSize = m_pFreeBucket->Push((STlsMemoryPoolNode<TData> *)pDelPtr);
				if (retSize == SBucket<TData, bucketSize, bucketCount>::kBucketSize)
				{
					m_pTlsSharedMemoryPool->Free(m_pFreeBucket->m_pTop);
					m_pFreeBucket->Clear();
				}
			}
			else
			{
				if (m_currentBucketIndex != -1)
				{
					int retSize = m_pAllocBucket[m_currentBucketIndex].Push((STlsMemoryPoolNode<TData> *)pDelPtr);
					if (retSize == SBucket<TData, bucketSize, bucketCount>::kBucketSize)
					{
						m_currentBucketIndex++;
					}
				}
				else
				{
					m_currentBucketIndex++;
					m_pAllocBucket[m_currentBucketIndex].Push((STlsMemoryPoolNode<TData> *)pDelPtr);
				}
			}
			m_usedCount--;
		}


	private:

		SBucket<TData, bucketSize, bucketCount> *m_pAllocBucket;
		int m_currentBucketIndex = -1;

		// 만약 MyBucket이 가득 찬 경우 -> FreeBucket에 반환
		// FreeBucket도 가득 찬 경우 공용 Bucket 풀에 반환
		SBucket<TData, bucketSize, bucketCount> *m_pFreeBucket;

		LONG	m_usedCount = 0;

		CTlsSharedMemoryPool<TData, bucketSize, bucketCount, useQueue> *m_pTlsSharedMemoryPool = nullptr;
	};

	template<typename TData, int bucketSize, int bucketCount, bool useQueue>
	class CTlsMemoryPoolManager
	{
	public:
		friend class CTlsMemoryPool<TData, bucketSize, bucketCount, useQueue>;

		CTlsMemoryPoolManager() noexcept
		{
			m_tlsMemoryPoolIdx = TlsAlloc();
			if (m_tlsMemoryPoolIdx == TLS_OUT_OF_INDEXES)
			{
				DWORD err = GetLastError();
				__debugbreak();
			}
		}

		~CTlsMemoryPoolManager() noexcept
		{
			// TLS 풀 정리 작업
		}

		LONG AllocTlsMemoryPoolIdx() noexcept
		{
			LONG idx = InterlockedIncrement(&m_tlsMemoryPoolsCurrentSize);
			return idx;
		}

		inline TData *Alloc() noexcept
		{
			__int64 tlsIndex = (__int64)TlsGetValue(m_tlsMemoryPoolIdx);
			if (tlsIndex == 0)
			{
				tlsIndex = AllocTlsMemoryPoolIdx();
				TlsSetValue(m_tlsMemoryPoolIdx, (LPVOID)tlsIndex);
				m_pTlsMemoryPools[tlsIndex] = new CTlsMemoryPool<TData, bucketSize, bucketCount, useQueue>(&m_tlsSharedMemoryPool);
			}

			TData *pPtr = m_pTlsMemoryPools[tlsIndex]->alloc();
			return pPtr;
		}

		inline void Free(TData *pFreePtr) noexcept
		{
			__int64 tlsIndex = (__int64)TlsGetValue(m_tlsMemoryPoolIdx);
			if (tlsIndex == 0)
			{
				tlsIndex = AllocTlsMemoryPoolIdx();
				TlsSetValue(m_tlsMemoryPoolIdx, (LPVOID)tlsIndex);
				m_pTlsMemoryPools[tlsIndex] = new CTlsMemoryPool<TData, bucketSize, bucketCount, useQueue>(&m_tlsSharedMemoryPool);
			}

			m_pTlsMemoryPools[tlsIndex]->free(pFreePtr);
		}

		inline LONG GetCapacity() noexcept
		{
			return m_tlsSharedMemoryPool.GetCapacity();
		}

		inline LONG GetUseCount() const noexcept
		{
			LONG usedSize = 0;
			for (int i = 1; i <= m_tlsMemoryPoolsCurrentSize; i++)
			{
				usedSize += m_pTlsMemoryPools[i]->m_usedCount;
			}

			return usedSize;
		}


	private:
		static constexpr int kThreadCount = 50;

		DWORD			m_tlsMemoryPoolIdx;

		// 인덱스 0번은 TLS의 초기값으로 사용이 불가능
		CTlsMemoryPool<TData, bucketSize, bucketCount, useQueue> *m_pTlsMemoryPools[kThreadCount];
		LONG					m_tlsMemoryPoolsCurrentSize = 0;

		CTlsSharedMemoryPool<TData, bucketSize, bucketCount, useQueue> m_tlsSharedMemoryPool =
			CTlsSharedMemoryPool<TData, bucketSize, bucketCount, useQueue>();
	};
}