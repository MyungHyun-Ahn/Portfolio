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

	bool Dequeue(T *t)
	{
		// 2�� �ۿ� Dequeue ���ϴ� ��Ȳ
		// -1 �ߴµ� 0�� �ִ� ��
		if (InterlockedDecrement(&m_iSize) < 0)
		{
			InterlockedIncrement(&m_iSize);
			return false;
		}

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
	CLFMemoryPool<Node> m_QueueNodePool = CLFMemoryPool<Node>(0, false);
	LONG				m_iSize = 0;
};

