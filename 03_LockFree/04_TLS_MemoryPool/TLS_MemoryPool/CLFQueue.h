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
	// InterlockedCompareExchange128�� ����� ������ 16����Ʈ ������ �ؾ���
	QUEUE_NODE_PTR next;
};

template<typename T, bool CAS2First = FALSE>
class CLFQueue
{
};

// CAS2�� ���� �����ϴ¹���
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

			// �ذ� 2
			// - Enqueue ��ܺ�

			// readTail�� ���ٸ� Tail�� ��ü

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

		// Enqueue ����
		InterlockedIncrement(&m_iSize);
	}

	bool Dequeue(T *t) noexcept
	{
		// 2�� �ۿ� Dequeue ���ϴ� ��Ȳ
		// -1 �ߴµ� 0�� �ִ� ��
		if (InterlockedDecrement(&m_iSize) < 0)
		{
			InterlockedIncrement(&m_iSize);
			return false;
		}

		PROFILE_BEGIN(1, "Dequeue");

		// ������� �� ���� ť�� ����� ��Ȳ�� ����

		ULONG_PTR readTail = m_pTail;
		Node *readTailAddr = (Node *)GetAddress(readTail);
		ULONG_PTR readTailNext = readTailAddr->next;

		while (true)
		{
			ULONG_PTR readHead = m_pHead;
			Node *readHeadAddr = (Node *)GetAddress(readHead);
			ULONG_PTR next = readHeadAddr->next;
			Node *nextAddr = (Node *)GetAddress(next);

			// Head->next NULL�� ���� ť�� ����� �� ��
			if (next == NULL)
			{
				continue;
			}
			else
			{
				// readHead == m_pHead �� m_pHead = next
				if (InterlockedCompareExchange(&m_pHead, next, readHead) == readHead)
				{
					// �����ߴµ� m_pTail�� next�� NULL�� �ƴ� ���?

					// DEQUEUE ���� �α�
					UINT64 index = InterlockedIncrement64(&logIndex);
					logging[index % LOG_MAX] = { index, GetCurrentThreadId(), DEQUEUE, 0, readHead, next, m_pTail, ((Node *)GetAddress(m_pTail))->next };

					// ���⼭ ������ ����� ������?
					*t = nextAddr->data;

					Node *readHeadAddr = (Node *)GetAddress(readHead);
					m_QueueNodePool.Free(readHeadAddr);
					break;
				}
			}
		}

		return true;
	}

private:
	ULONG_PTR			m_pHead = NULL;
	ULONG_PTR			m_pTail = NULL;
	ULONG_PTR			m_ullCurrentIdentifier = 0; // ABA ������ �ذ��ϱ� ���� �ĺ���
	inline static CLFMemoryPool<Node> m_QueueNodePool = CLFMemoryPool<Node>(0, false);
	LONG				m_iSize = 0;
};

// CAS1�� ���� �����ϴ� ����
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
		// ť �ĺ��ڷ� ���� �ٸ� NULL ���� ���
		m_NULL = InterlockedIncrement(&s_iQueueIdentifier) % 0xFFFF; // �������� ������ ��ȣ ������ŭ�� ���

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

				break; // ������� �ߴٸ� break;
			}

		}

		// Enqueue ����
		InterlockedIncrement(&m_iSize);
	}

	bool Dequeue(T *t) noexcept
	{
		// 2�� �ۿ� Dequeue ���ϴ� ��Ȳ
		// -1 �ߴµ� 0�� �ִ� ��
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

			// Head�� Tail�� ���ٸ� �ѹ� �о��ְ� Dequeue ����
			if (readHead == readTail)
			{
				Node *readTailAddr = (Node *)GetAddress(readTail);
				ULONG_PTR readTailNext = readTailAddr->next;
				InterlockedCompareExchange(&m_pTail, readTailNext, readTail);
			}

			// Head->next NULL�� ���� ť�� ����� �� ��
			if (next == NULL)
			{
				continue;
			}
			else
			{
				if (InterlockedCompareExchange(&m_pHead, next, readHead) == readHead)
				{
					*t = nextAddr->data;

					Node *readHeadAddr = (Node *)GetAddress(readHead);
					m_QueueNodePool.Free(readHeadAddr);
					break;
				}
			}
		}

		return true;
	}

private:
	ULONG_PTR			m_pHead = NULL;
	ULONG_PTR			m_pTail = NULL;
	ULONG_PTR			m_ullCurrentIdentifier = 0; // ABA ������ �ذ��ϱ� ���� �ĺ���
	inline static CTLSMemoryPoolManager<Node> m_QueueNodePool = CTLSMemoryPoolManager<Node>();
	LONG				m_iSize = 0;

	ULONG_PTR			m_NULL;
	inline static LONG	s_iQueueIdentifier = 0;
};


// Double CAS�� �ذ�
//// CAS1�� ���� �����ϴ� ����
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
//			// �ĺ��� Ȯ���ϰ� ���� ����
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
//		// Enqueue ����
//		InterlockedIncrement(&m_iSize);
//	}
//
//	bool Dequeue(T *t) noexcept
//	{
//		// 2�� �ۿ� Dequeue ���ϴ� ��Ȳ
//		// -1 �ߴµ� 0�� �ִ� ��
//		if (InterlockedDecrement(&m_iSize) < 0)
//		{
//			InterlockedIncrement(&m_iSize);
//			return false;
//		}
//
//		PROFILE_BEGIN(1, "Dequeue");
//
//		// ������� �� ���� ť�� ����� ��Ȳ�� ����
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
//			// Head->next NULL�� ���� ť�� ����� �� ��
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
//	LONG				m_ullCurrentIdentifier = 0; // ABA ������ �ذ��ϱ� ���� �ĺ���
//	inline static CTLSMemoryPoolManager<Node> m_QueueNodePool = CTLSMemoryPoolManager<Node>();
//	// CLFMemoryPool<Node> m_QueueNodePool = CLFMemoryPool<Node>(0, false);
//	LONG				m_iSize = 0;
//	LONG				m_iQueueIdent;
//
//	inline static LONG	s_iQueueIdentifier = 0;
//};

