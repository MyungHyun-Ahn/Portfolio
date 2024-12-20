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

template<typename DATA>
struct QueueNode
{
	DATA data;
	ULONG_PTR next;
};

template<typename T>
class CLFQueue
{
public:
	using Node = QueueNode<T>;

	CLFQueue() 
		: m_iSize(0)
	{
		UINT_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
		Node *pHead = m_QueueNodePool.Alloc();
		pHead->next = NULL;
		m_pHead = CombineIdentAndAddr(ident, (ULONG_PTR)pHead);
		m_pTail = m_pHead;
	}

	void Enqueue(T t)
	{
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

	bool Dequeue(T *t)
	{
		// 2번 밖에 Dequeue 안하는 상황
		// -1 했는데 0은 있는 거
		if (InterlockedDecrement(&m_iSize) < 0)
		{
			InterlockedIncrement(&m_iSize);
			return false;
		}

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

private:
	ULONG_PTR			m_pHead = NULL;
	ULONG_PTR			m_pTail = NULL;
	ULONG_PTR			m_ullCurrentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
	CLFMemoryPool<Node> m_QueueNodePool = CLFMemoryPool<Node>(0, false);
	LONG				m_iSize = 0;
};

