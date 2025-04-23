#pragma once

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
	ULONG_PTR next;
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
				*t = nextAddr->data;
				if (InterlockedCompareExchange(&m_pHead, next, readHead) == readHead)
				{
					// 여기서 문제가 생길것 같은데?

					Node *readHeadAddr = (Node *)GetAddress(readHead);
					m_QueueNodePool.Free(readHeadAddr);
					break;
				}
			}
		}

		return true;
	}

	inline LONG GetUseSize() const noexcept { return m_iSize; }

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
			UINT_PTR readEnqueueId = m_ENQUEUE_ID;

			// 순서 꼬인 문제를 해결하기 위함
			// 결국 Cas1과 Cas2가 묶인 꼴이 나옴
			if (readEnqueueId != m_ENQUEUE_ID)
				continue;

			if (InterlockedCompareExchange(&readTailAddr->next, combinedNode, m_NULL) == m_NULL)
			{
				InterlockedIncrement(&m_ENQUEUE_ID);
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

		// PROFILE_BEGIN(1, "Dequeue");

		while (true)
		{
			ULONG_PTR readHead = m_pHead;
			Node *readHeadAddr = (Node *)GetAddress(readHead);
			ULONG_PTR next = readHeadAddr->next;
			Node *nextAddr = (Node *)GetAddress(next);

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

	inline LONG GetUseSize() const noexcept { return m_iSize; }

private:
	ULONG_PTR			m_pHead = NULL;
	ULONG_PTR			m_pTail = NULL;
	ULONG_PTR			m_ullCurrentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
	inline static CTLSMemoryPoolManager<Node, 256, 4> m_QueueNodePool = CTLSMemoryPoolManager<Node, 256, 4>();
	LONG				m_iSize = 0;

	ULONG_PTR			m_NULL;
	ULONG_PTR			m_ENQUEUE_ID = 0;

	inline static LONG	s_iQueueIdentifier = 0;
};