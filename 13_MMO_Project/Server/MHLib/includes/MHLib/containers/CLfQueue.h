#pragma once

namespace MHLib::containers
{
	using namespace MHLib::memory;

	struct alignas(16) SQueueNodePtr
	{
		ULONG_PTR m_address;
		ULONG_PTR m_queueIdent;
	};

	template<typename TData, bool cas2First = TRUE>
	struct SQueueNode
	{

	};

	template<typename TData>
	struct SQueueNode<TData, TRUE>
	{
		TData m_data;
		ULONG_PTR m_next;
	};

	template<typename TData>
	struct SQueueNode<TData, FALSE>
	{
		TData m_data;
		ULONG_PTR m_next;
	};

	template<FundamentalOrPointer TObject, bool cas2First = FALSE>
	class CLfQueue
	{
	};

	// CAS2을 먼저 수행하는버전
	template<FundamentalOrPointer TObject>
	class CLfQueue<TObject, TRUE>
	{
	public:
		using SNode = SQueueNode<TObject>;

		CLfQueue() noexcept
			: m_size(0)
		{
			ULONG_PTR ident = InterlockedIncrement(&m_currentIdentifier);
			SNode *pHead = sm_queueNodePool.Alloc();
			pHead->m_next = NULL;
			m_head = CombineIdentAndAddr(ident, (ULONG_PTR)pHead);
			m_tail = m_head;
		}

		void Enqueue(TObject t) noexcept
		{
			// PROFILE_BEGIN(1, "Enqueue");

			UINT_PTR ident = InterlockedIncrement(&m_currentIdentifier);
			SNode *pNewNode = sm_queueNodePool.Alloc();
			pNewNode->m_data = t;
			pNewNode->m_next = NULL;
			ULONG_PTR combinedNode = CombineIdentAndAddr(ident, (ULONG_PTR)pNewNode);

			while (true)
			{
				ULONG_PTR readTail = m_tail;
				SNode *pReadTailAddr = (SNode *)GetAddress(readTail);
				ULONG_PTR next = pReadTailAddr->m_next;

				// 해결 2
				// - Enqueue 상단부

				// readTail과 같다면 Tail을 교체

				if (InterlockedCompareExchange(&m_tail, combinedNode, readTail) == readTail)
				{
					if (InterlockedCompareExchange(&pReadTailAddr->m_next, combinedNode, NULL) == NULL)
					{
						break;
					}
					else
					{
						__debugbreak();
					}
				}
			}

			// Enqueue 성공
			InterlockedIncrement(&m_size);
		}

		bool Dequeue(TObject *outData) noexcept
		{
			// 2번 밖에 Dequeue 안하는 상황
			// -1 했는데 0은 있는 거
			if (InterlockedDecrement(&m_size) < 0)
			{
				InterlockedIncrement(&m_size);
				return false;
			}

			while (true)
			{
				ULONG_PTR readHead = m_head;
				SNode *pReadHeadAddr = (SNode *)GetAddress(readHead);
				ULONG_PTR next = pReadHeadAddr->m_next;
				SNode *pNextAddr = (SNode *)GetAddress(next);

				// Head->next NULL인 경우는 큐가 비었을 때 뿐
				if (next == NULL)
				{
					continue;
				}
				else
				{
					// readHead == m_pHead 면 m_pHead = next
					*outData = pNextAddr->m_data;
					if (InterlockedCompareExchange(&m_head, next, readHead) == readHead)
					{
						// 여기서 문제가 생길것 같은데?

						SNode *pReadHeadAddr = (SNode *)GetAddress(readHead);
						sm_queueNodePool.Free(pReadHeadAddr);
						break;
					}
				}
			}

			return true;
		}

		inline LONG GetUseSize() const noexcept { return m_size; }

	private:
		ULONG_PTR			m_head = NULL;
		ULONG_PTR			m_tail = NULL;
		ULONG_PTR			m_currentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
		inline static CTlsMemoryPoolManager<SNode, 256, 4> sm_queueNodePool = CTlsMemoryPoolManager<SNode, 256, 4>();
		LONG				m_size = 0;
	};

	// CAS1를 먼저 수행하는 버전
	template<FundamentalOrPointer TObject>
	class CLfQueue<TObject, FALSE>
	{
	public:
		using SNode = SQueueNode<TObject>;

		CLfQueue() noexcept
			: m_size(0)
		{
			// 큐 식별자로 각기 다른 NULL 값을 사용
			m_null = InterlockedIncrement(&sm_queueIdentifier) % 0xFFFF; // 윈도우의 페이지 보호 영역만큼만 사용

			UINT_PTR ident = InterlockedIncrement(&m_currentIdentifier);
			SNode *pHead = sm_queueNodePool.Alloc();
			pHead->m_next = m_null;
			m_head = CombineIdentAndAddr(ident, (ULONG_PTR)pHead);
			m_tail = m_head;
		}

		void Enqueue(TObject t) noexcept
		{
			UINT_PTR ident = InterlockedIncrement(&m_currentIdentifier);
			SNode *pNewNode = sm_queueNodePool.Alloc();
			pNewNode->m_data = t;
			pNewNode->m_next = m_null;
			ULONG_PTR combinedNode = CombineIdentAndAddr(ident, (ULONG_PTR)pNewNode);

			while (true)
			{
				ULONG_PTR readTail = m_tail;
				SNode *pReadTailAddr = (SNode *)GetAddress(readTail);
				ULONG_PTR next = pReadTailAddr->m_next;
				UINT_PTR readEnqueueId = m_enqueueId;

				// 순서 꼬인 문제를 해결하기 위함
				// 결국 Cas1과 Cas2가 묶인 꼴이 나옴
				if (readEnqueueId != m_enqueueId)
					continue;

				if (InterlockedCompareExchange(&pReadTailAddr->m_next, combinedNode, m_null) == m_null)
				{
					InterlockedIncrement(&m_enqueueId);
					InterlockedCompareExchange(&m_tail, combinedNode, readTail);
					break; // 여기까지 했다면 break;
				}
			}

			// Enqueue 성공
			InterlockedIncrement(&m_size);
		}

		bool Dequeue(TObject *outData) noexcept
		{
			// 2번 밖에 Dequeue 안하는 상황
			// -1 했는데 0은 있는 거
			if (InterlockedDecrement(&m_size) < 0)
			{
				InterlockedIncrement(&m_size);
				return false;
			}

			// PROFILE_BEGIN(1, "Dequeue");

			while (true)
			{
				ULONG_PTR readHead = m_head;
				SNode *pReadHeadAddr = (SNode *)GetAddress(readHead);
				ULONG_PTR next = pReadHeadAddr->m_next;
				SNode *pNextAddr = (SNode *)GetAddress(next);

				// Head->next NULL인 경우는 큐가 비었을 때 뿐
				if (next == m_null)
				{
					continue;
				}
				else
				{
					// next가 0xFFFF보다 작으면 잘못 읽은 것임
					if (next < 0xFFFF)
					{
						continue;
					}

					// 먼저 읽고 CAS 진행
					*outData = pNextAddr->m_data;
					if (InterlockedCompareExchange(&m_head, next, readHead) == readHead)
					{
						SNode *pReadHeadAddr = (SNode *)GetAddress(readHead);
						sm_queueNodePool.Free(pReadHeadAddr);
						break;
					}
				}
			}

			return true;
		}

		inline LONG GetUseSize() const noexcept { return m_size; }

	private:
		ULONG_PTR			m_head = NULL;
		ULONG_PTR			m_tail = NULL;
		ULONG_PTR			m_currentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
		inline static CTlsMemoryPoolManager<SNode, 256, 4> sm_queueNodePool = CTlsMemoryPoolManager<SNode, 256, 4>();
		LONG				m_size = 0;

		ULONG_PTR			m_null;
		ULONG_PTR			m_enqueueId = 0;

		inline static LONG	sm_queueIdentifier = 0;
	};
}