#pragma once

// #define USE_LFQUEUE_LOG

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

extern QueueDebug QueueLogging[LOG_MAX];
extern LONG64 QueueLogIndex;

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
		Node *pHead = s_QueueNodePool.Alloc();
		pHead->next = NULL;
		m_pHead = CombineIdentAndAddr(ident, (ULONG_PTR)pHead);
		m_pTail = m_pHead;
	}

	void Enqueue(T t)
	{
		UINT_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
		Node *newNode = s_QueueNodePool.Alloc();
		newNode->data = t;
		newNode->next = NULL;
		ULONG_PTR combinedNode = CombineIdentAndAddr(ident, (ULONG_PTR)newNode);

		while (true)
		{
			ULONG_PTR readTail = m_pTail;
			Node *readTailAddr = (Node *)GetAddress(readTail);
			ULONG_PTR next = readTailAddr->next;

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
			return false; // 큐가 비었음
		}

		// 여기까지 온 경우는 큐가 비었을 상황은 없음

		while (true)
		{
			ULONG_PTR readHead = m_pHead;
			Node *readHeadAddr = (Node *)GetAddress(readHead);
			ULONG_PTR next = readHeadAddr->next;
			Node *nextAddr = (Node *)GetAddress(next);

			// Head->next NULL인 경우
			//  - head를 잘못본 경우뿐
			//  - continue 해서 다시봐줌
			if (next == NULL)
			{
				continue;
			}
			else
			{
				// readHead == m_pHead 면 m_pHead = next
				if (InterlockedCompareExchange(&m_pHead, next, readHead) == readHead)
				{
					*t = nextAddr->data;
					Node *readHeadAddr = (Node *)GetAddress(readHead);
					s_QueueNodePool.Free(readHeadAddr);
					break;
				}
			}
		}

		return true;
	}

	inline LONG GetUseSize() { return m_iSize; }

private:
	ULONG_PTR			m_pHead = NULL;
	ULONG_PTR			m_pTail = NULL;
	ULONG_PTR			m_ullCurrentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
	inline static CLFMemoryPool<Node> s_QueueNodePool = CLFMemoryPool<Node>(0, false);
	LONG				m_iSize = 0;
};