#pragma once

#define ENQUEUE_CAS1 0
#define ENQUEUE_CAS2 1
#define DEQUEUE 2

struct QueueDebug
{
	UINT64 index;
	DWORD threadId;
	USHORT operation;
	USHORT failedReason;
	ULONG_PTR addr1;
	ULONG_PTR addr2;
	ULONG_PTR addr3;
	ULONG_PTR addr4;
};

#define LOG_MAX 200000

extern QueueDebug logging[200000];
extern LONG64 logIndex;

struct alignas(16) QUEUE_NODE_PTR
{
	ULONG_PTR ptr;
	ULONG_PTR queueIdent;
};

template<typename DATA, bool CAS2First = TRUE>
struct QueueNode
{

};

template<typename DATA>
struct QueueNode<DATA, TRUE>
{
	DATA data;
	ULONG_PTR next;
};

template<typename DATA>
struct QueueNode<DATA, FALSE>
{
	DATA data;
	// InterlockedCompareExchange128에 사용할 변수는 16바이트 경계맞춤 해야함
	QUEUE_NODE_PTR next;
};

template<typename T, bool CAS2First = FALSE>
class CLFQueue
{
};

// CAS2을 먼저 수행하는버전
template<typename T>
class CLFQueue<T, TRUE>
{
	static_assert(std::is_fundamental<T>::value || std::is_pointer<T>::value,
		"T must be a fundamental type or a pointer type.");

public:
	using Node = QueueNode<T>;

	CLFQueue() noexcept
		: m_iSize(0)
	{
		ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
		Node *pHead = m_QueueNodePool.Alloc();
		pHead->next = NULL;
		m_pHead = CombineIdentAndAddr(ident, (ULONG_PTR)pHead);
		m_pTail = m_pHead;
	}

	void Enqueue(T t) noexcept
	{
		PROFILE_BEGIN(1, "Enqueue");

		UINT_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
		Node *newNode = m_QueueNodePool.Alloc();
		newNode->data = t;
		newNode->next = NULL;
		ULONG_PTR combinedNode = CombineIdentAndAddr(ident, (ULONG_PTR)newNode);

		while (true)
		{
			ULONG_PTR readTail = m_pTail;
			Node *readTailAddr = (Node *)GetAddress(readTail);
			ULONG_PTR next = readTailAddr->next;

			// 해결 2
			// - Enqueue 상단부

			// readTail과 같다면 Tail을 교체

			if (InterlockedCompareExchange(&m_pTail, combinedNode, readTail) == readTail)
			{
				if (InterlockedCompareExchange(&readTailAddr->next, combinedNode, NULL) == NULL)
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
		InterlockedIncrement(&m_iSize);
	}

	bool Dequeue(T *t) noexcept
	{
		// 2번 밖에 Dequeue 안하는 상황
		// -1 했는데 0은 있는 거
		if (InterlockedDecrement(&m_iSize) < 0)
		{
			InterlockedIncrement(&m_iSize);
			return false;
		}

		PROFILE_BEGIN(1, "Dequeue");

		// 여기까지 온 경우는 큐가 비었을 상황은 없음

		ULONG_PTR readTail = m_pTail;
		Node *readTailAddr = (Node *)GetAddress(readTail);
		ULONG_PTR readTailNext = readTailAddr->next;

		while (true)
		{
			ULONG_PTR readHead = m_pHead;
			Node *readHeadAddr = (Node *)GetAddress(readHead);
			ULONG_PTR next = readHeadAddr->next;
			Node *nextAddr = (Node *)GetAddress(next);

			// Head->next NULL인 경우는 큐가 비었을 때 뿐
			if (next == NULL)
			{
				continue;
			}
			else
			{
				// readHead == m_pHead 면 m_pHead = next
				if (InterlockedCompareExchange(&m_pHead, next, readHead) == readHead)
				{
					// 성공했는데 m_pTail의 next가 NULL이 아닌 경우?

					// DEQUEUE 성공 로그
					UINT64 index = InterlockedIncrement64(&logIndex);
					logging[index % LOG_MAX] = { index, GetCurrentThreadId(), DEQUEUE, 0, readHead, next, m_pTail, ((Node *)GetAddress(m_pTail))->next };

					// 여기서 문제가 생길것 같은데?
					*t = nextAddr->data;

					Node *readHeadAddr = (Node *)GetAddress(readHead);
					m_QueueNodePool.Free(readHeadAddr);
					break;
				}
			}
		}

		return true;
	}

	inline LONG GetUseSize() noexcept { return m_iSize; }

private:
	ULONG_PTR			m_pHead = NULL;
	ULONG_PTR			m_pTail = NULL;
	ULONG_PTR			m_ullCurrentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
	inline static CLFMemoryPool<Node> m_QueueNodePool = CLFMemoryPool<Node>(0, false);
	LONG				m_iSize = 0;
};

// CAS1를 먼저 수행하는 버전
template<typename T>
class CLFQueue<T, FALSE>
{
	static_assert(std::is_fundamental<T>::value || std::is_pointer<T>::value,
		"T must be a fundamental type or a pointer type.");

public:
	using Node = QueueNode<T>;

	CLFQueue() noexcept
		: m_iSize(0)
	{
		// 큐 식별자로 각기 다른 NULL 값을 사용
		m_NULL = InterlockedIncrement(&s_iQueueIdentifier) % 0xFFFF; // 윈도우의 페이지 보호 영역만큼만 사용

		UINT_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
		Node *pHead = m_QueueNodePool.Alloc();
		pHead->next = m_NULL;
		m_pHead = CombineIdentAndAddr(ident, (ULONG_PTR)pHead);
		m_pTail = m_pHead;
	}

	void Enqueue(T t) noexcept
	{
		PROFILE_BEGIN(1, "Enqueue");

		UINT_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
		Node *newNode = m_QueueNodePool.Alloc();
		newNode->data = t;
		newNode->next = m_NULL;
		ULONG_PTR combinedNode = CombineIdentAndAddr(ident, (ULONG_PTR)newNode);

		while (true)
		{
			ULONG_PTR readTail = m_pTail;
			Node *readTailAddr = (Node *)GetAddress(readTail);
			ULONG_PTR next = readTailAddr->next;

			if (next != m_NULL)
			{
				InterlockedCompareExchange(&m_pTail, next, readTail);
				continue;
			}

			if (InterlockedCompareExchange(&readTailAddr->next, combinedNode, m_NULL) == m_NULL)
			{
				InterlockedCompareExchange(&m_pTail, combinedNode, readTail);

				break; // 여기까지 했다면 break;
			}

		}

		// Enqueue 성공
		InterlockedIncrement(&m_iSize);
	}

	bool Dequeue(T *t) noexcept
	{
		// 2번 밖에 Dequeue 안하는 상황
		// -1 했는데 0은 있는 거
		if (InterlockedDecrement(&m_iSize) < 0)
		{
			InterlockedIncrement(&m_iSize);
			return false;
		}

		PROFILE_BEGIN(1, "Dequeue");

		while (true)
		{
			ULONG_PTR readHead = m_pHead;
			ULONG_PTR readTail = m_pTail;
			Node *readHeadAddr = (Node *)GetAddress(readHead);
			ULONG_PTR next = readHeadAddr->next;
			Node *nextAddr = (Node *)GetAddress(next);

			// Head와 Tail이 같다면 한번 밀어주고 Dequeue 수행
			if (readHead == readTail)
			{
				Node *readTailAddr = (Node *)GetAddress(readTail);
				ULONG_PTR readTailNext = readTailAddr->next;
				InterlockedCompareExchange(&m_pTail, readTailNext, readTail);
			}

			// Head->next NULL인 경우는 큐가 비었을 때 뿐
			if (next == m_NULL)
			{
				continue;
			}
			else
			{
				// next가 0xFFFF보다 작으면 잘못 읽은 것임
				if (next < 0xFFFF)
				{
					g_Logger->WriteLog(L"SYSTEM", L"LockFreeQueue", LOG_LEVEL::ERR, L"0xFFFF");
					continue;
				}

				// 먼저 읽고 CAS 진행
				*t = nextAddr->data;
				if (InterlockedCompareExchange(&m_pHead, next, readHead) == readHead)
				{
					Node *readHeadAddr = (Node *)GetAddress(readHead);
					m_QueueNodePool.Free(readHeadAddr);
					break;
				}
			}
		}

		return true;
	}

	inline LONG GetUseSize() noexcept { return m_iSize; }

private:
	ULONG_PTR			m_pHead = NULL;
	ULONG_PTR			m_pTail = NULL;
	ULONG_PTR			m_ullCurrentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
	inline static CTLSMemoryPoolManager<Node> m_QueueNodePool = CTLSMemoryPoolManager<Node>();
	LONG				m_iSize = 0;

	ULONG_PTR			m_NULL;
	inline static LONG	s_iQueueIdentifier = 0;
};


// Double CAS로 해결
//// CAS1를 먼저 수행하는 버전
//template<typename T>
//class CLFQueue<T, FALSE>
//{
//	static_assert(std::is_fundamental<T>::value || std::is_pointer<T>::value,
//		"T must be a fundamental type or a pointer type.");
//
//public:
//	using Node = QueueNode<T, FALSE>;
//
//	CLFQueue() noexcept
//		: m_iSize(0)
//	{
//		m_iQueueIdent = InterlockedIncrement(&s_iQueueIdentifier);
//		ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
//		m_NULL.ptr = NULL;
//		m_NULL.queueIdent = m_iQueueIdent;
//
//		Node *pHead = m_QueueNodePool.Alloc();
//		pHead->next = m_NULL;
//
//		ULONG_PTR combinePtr = CombineIdentAndAddr(ident, (ULONG_PTR)pHead);
//
//		m_pHead = combinePtr;
//		m_pTail = m_pHead;
//	}
//
//	void Enqueue(T t) noexcept
//	{
//		PROFILE_BEGIN(1, "Enqueue");
//
//		ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
//		Node *newNode = m_QueueNodePool.Alloc();
//		newNode->data = t;
//		newNode->next = m_NULL;
//
//		ULONG_PTR combinedNode = CombineIdentAndAddr(ident, (ULONG_PTR)newNode);
//
//		while (true)
//		{
//			ULONG_PTR readTail = m_pTail;
//			Node *readTailAddr = (Node *)GetAddress(readTail);
//			QUEUE_NODE_PTR next = readTailAddr->next;
//
//			if (next.ptr != NULL)
//			{
//				InterlockedCompareExchange(&m_pTail, next.ptr, readTail);
//				continue;
//			}
//
//			// 식별자 확인하고 실패 유도
//			if (m_iQueueIdent != next.queueIdent)
//				continue;
//
//			if (InterlockedCompareExchange128((LONG64 *)&readTailAddr->next, (LONG64)m_iQueueIdent, (LONG64)combinedNode, (LONG64 *)&next) == FALSE)
//			{
//				continue;
//			}
//
//			// CAS02
//			InterlockedCompareExchange(&m_pTail, combinedNode, readTail);
//			break;
//		}
//
//		// Enqueue 성공
//		InterlockedIncrement(&m_iSize);
//	}
//
//	bool Dequeue(T *t) noexcept
//	{
//		// 2번 밖에 Dequeue 안하는 상황
//		// -1 했는데 0은 있는 거
//		if (InterlockedDecrement(&m_iSize) < 0)
//		{
//			InterlockedIncrement(&m_iSize);
//			return false;
//		}
//
//		PROFILE_BEGIN(1, "Dequeue");
//
//		// 여기까지 온 경우는 큐가 비었을 상황은 없음
//		while (true)
//		{
//			ULONG_PTR readHead = m_pHead;
//			ULONG_PTR readTail = m_pTail;
//			Node *readHeadAddr = (Node *)GetAddress(readHead);
//			ULONG_PTR next = readHeadAddr->next.ptr;
//			Node *nextAddr = (Node *)GetAddress(next);
//
//			if (readHead == readTail)
//			{
//				Node *readTailAddr = (Node *)GetAddress(readTail);
//				ULONG_PTR readTailNext = readTailAddr->next.ptr;
//				InterlockedCompareExchange(&m_pTail, readTailNext, readTail);
//			}
//
//			// Head->next NULL인 경우는 큐가 비었을 때 뿐
//			if (next == NULL)
//			{
//				continue;
//			}
//			else
//			{
//				if (InterlockedCompareExchange(&m_pHead, next, readHead) == readHead)
//				{
//					*t = nextAddr->data;
//
//					Node *readHeadAddr = (Node *)GetAddress(readHead);
//					m_QueueNodePool.Free(readHeadAddr);
//					break;
//				}
//			}
//		}
//
//		return true;
//	}
//
//private:
//	ULONG_PTR			m_pHead;
//	ULONG_PTR			m_pTail;
//	QUEUE_NODE_PTR      m_NULL;
//	LONG				m_ullCurrentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
//	inline static CTLSMemoryPoolManager<Node> m_QueueNodePool = CTLSMemoryPoolManager<Node>();
//	// CLFMemoryPool<Node> m_QueueNodePool = CLFMemoryPool<Node>(0, false);
//	LONG				m_iSize = 0;
//	LONG				m_iQueueIdent;
//
//	inline static LONG	s_iQueueIdentifier = 0;
//};

