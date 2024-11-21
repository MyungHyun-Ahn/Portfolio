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
			while (next != NULL)
			{
				InterlockedCompareExchange(&m_pTail, next, readTail);
				readTail = m_pTail; // �ٽ� �б�
				readTailAddr = (Node *)GetAddress(readTail);
				next = readTailAddr->next;
			}


			// CAS 01
			// �о�� Tail�� next�� NULL�̶�� combinedNode�� �ٲٱ�
			if (InterlockedCompareExchange(&readTailAddr->next, combinedNode, NULL) == NULL)
			{
#ifdef USE_LFQUEUE_LOG
				// �ε��� �߱�
				UINT64 index = InterlockedIncrement64(&QueueLogIndex);
				QueueLogging[index % LOG_MAX] = { index, GetCurrentThreadId(), ENQUEUE_CAS1, 0, readTail, m_pTail, combinedNode, next };
#endif
				// CAS 02
				// �о�� Tail�� ���ٸ� m_pTail�� �ٲٱ�
#ifdef USE_LFQUEUE_LOG
				if (InterlockedCompareExchange(&m_pTail, combinedNode, readTail) != readTail)
				{
					// CAS 02 ���� �α�
					// CAS 02 ���н� newNode->next üũ NULL�� �ƴ��� -> NULL�� �ƴ� ����
					UINT64 index = InterlockedIncrement64(&QueueLogIndex);
					QueueLogging[index % LOG_MAX] = { index, GetCurrentThreadId(), ENQUEUE_CAS2, 2, readTail, m_pTail, combinedNode, next };

				}
				else
				{
					// ���� �α�
					// Tail�� �ٲ���
					UINT64 index = InterlockedIncrement64(&QueueLogIndex);
					QueueLogging[index % LOG_MAX] = { index, GetCurrentThreadId(), ENQUEUE_CAS2, 0, readTail, m_pTail, combinedNode, next };
#
				}
#else
				InterlockedCompareExchange(&m_pTail, combinedNode, readTail);
#endif
				break; // ������� �ߴٸ� break;
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
			return false; // ť�� �����
		}

		// ������� �� ���� ť�� ����� ��Ȳ�� ����

		while (true)
		{
			ULONG_PTR readHead = m_pHead;
			Node *readHeadAddr = (Node *)GetAddress(readHead);
			ULONG_PTR next = readHeadAddr->next;
			Node *nextAddr = (Node *)GetAddress(next);

			ULONG_PTR readTail = m_pTail;
			Node *readTailAddr = (Node *)GetAddress(readTail);
			ULONG_PTR readTailNext = readTailAddr->next;

			while (readTailNext != NULL)
			{
				InterlockedCompareExchange(&m_pTail, readTailNext, readTail);
				readTail = m_pTail; // �ٽ� �б�
				readTailAddr = (Node *)GetAddress(readTail);
				readTailNext = readTailAddr->next;
			}

			// Head->next NULL�� ���
			//  - head�� �߸��� ����
			//  - continue �ؼ� �ٽú���
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
#ifdef USE_LFQUEUE_LOG
					// DEQUEUE ���� �α�
					UINT64 index = InterlockedIncrement64(&QueueLogIndex);
					QueueLogging[index % LOG_MAX] = { index, GetCurrentThreadId(), DEQUEUE, 0, readHead, next, m_pTail, ((Node *)GetAddress(m_pTail))->next };
#endif

					*t = nextAddr->data;
					Node *readHeadAddr = (Node *)GetAddress(readHead);
					m_QueueNodePool.Free(readHeadAddr);
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
	ULONG_PTR			m_ullCurrentIdentifier = 0; // ABA ������ �ذ��ϱ� ���� �ĺ���
	CLFMemoryPool<Node> m_QueueNodePool = CLFMemoryPool<Node>(0, false);
	LONG				m_iSize = 0;
};