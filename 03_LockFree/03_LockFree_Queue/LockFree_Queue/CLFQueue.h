#pragma once

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
	using Node = QueueNode<DATA>;

	CLFQueue() 
		: m_iSize(0)
	{
		UINT_PTR ident = InterlockedIncrement64(&m_ullCurrentIdentifier);
		Node *pHead = m_QueueNodePool.Alloc();
		pHead->next = NULL;
		m_pHead = CombineIdentAndAddr(ident, (ULONG_PTR)pHead);
		m_pTail = m_pHead;
	}

	void Enqueue(T t)
	{
		UINT_PTR ident = InterlockedIncrement64(&m_ullCurrentIdentifier);
		Node *newNode = m_QueueNodePool.Alloc();
		newNode->data = t;
		newNode->next = NULL;
		ULONG_PTR combinedNode = CombineIdentAndAddr(ident, (ULONG_PTR)newNode);

		while (true)
		{
			ULONG_PTR readTail = m_pTail;
			Node *readTailAddr = (Node *)GetAddress(m_pTail);
			ULONG_PTR next = readTailAddr->next;

			// 읽어온 Tail의 next가 NULL이라면 combinedNode로 바꾸기
			if (InterlockedCompareExchange(&readTailAddr->next, combinedNode, NULL) == next)
			{
				// 읽어온 Tail과 같다면 m_pTail을 바꾸기
				InterlockedCompareExchange(&m_pTail, combinedNode, readTail);
				break; // 여기까지 했다면 break;
			}

		}

		// Enqueue 성공
		InterlockedIncrement(&m_iSize);
	}

	bool Dequeue(T *t)
	{
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

			if (next == NULL)
			{
				__debugbreak();
				return false;
			}
			else
			{
				if (InterlockedCompareExchange(&m_pHead, next, readHead) == readHead)
				{
					Node *nextAddr = (Node *)GetAddress(next);
					t = nextAddr->data;
					m_QueueNodePool.Free(readHeadAddr);
					break;
				}
			}
		}

		InterlockedDecrement(&m_iSize);
		return true;
	}

private:
	ULONG_PTR			m_pHead = NULL;
	ULONG_PTR			m_pTail = NULL;
	ULONG_PTR			m_ullCurrentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
	CLFMemoryPool<Node> m_QueueNodePool = CLFMemoryPool<Node>(1000, false);
	LONG				m_iSize = 0;
};

