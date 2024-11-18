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

			// CAS 01
			// �о�� Tail�� next�� NULL�̶�� combinedNode�� �ٲٱ�
			if (InterlockedCompareExchange(&readTailAddr->next, combinedNode, NULL) == NULL)
			{
				// �ε��� �߱�
				UINT64 index = InterlockedIncrement64(&logIndex);
				logging[index % LOG_MAX] = { index, GetCurrentThreadId(), ENQUEUE_CAS1, 0, readTail, m_pTail, combinedNode, next };

				// CAS 02
				// �о�� Tail�� ���ٸ� m_pTail�� �ٲٱ�
				if (InterlockedCompareExchange(&m_pTail, combinedNode, readTail) != readTail)
				{
					// CAS 02 ���� �α�
					// CAS 02 ���н� newNode->next üũ NULL�� �ƴ��� -> NULL�� �ƴ� ����
					UINT64 index = InterlockedIncrement64(&logIndex);
					logging[index % LOG_MAX] = { index, GetCurrentThreadId(), ENQUEUE_CAS2, 2, readTail, m_pTail, combinedNode, next };

					// CAS 02 ����
					// __debugbreak();
					break;

				}
				else
				{
					// ���� �α�
					// Tail�� �ٲ���
					UINT64 index = InterlockedIncrement64(&logIndex);
					logging[index % LOG_MAX] = { index, GetCurrentThreadId(), ENQUEUE_CAS2, 0, readTail, m_pTail, combinedNode, next };
					break;
				}

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
			// �÷ȴµ��� 0���� ������ 
			if (InterlockedIncrement(&m_iSize) <= 0)
				return false; // ť�� �����
		}

		// ������� �� ���� ť�� ����� ��Ȳ�� ����

		while (true)
		{
			ULONG_PTR readHead = m_pHead;
			Node *readHeadAddr = (Node *)GetAddress(readHead);
			ULONG_PTR next = readHeadAddr->next;

			ULONG_PTR readTail = m_pTail;
			Node *readTailAddr = (Node *)GetAddress(readTail);
			ULONG_PTR readTailNext = readTailAddr->next;
			// Head->next NULL�� ���� ť�� ����� �� ��
			if (next == NULL)
			{
				// ���� ���� ���� head->next �� �߸����� �ִ� ���
				// th01 readHead = 1 // ���� �� �а� ������
				// th02 readHead = 1
				// th01 �� Pop �� �Ϸ� �ع���
				// th01 �� �ٽ� Push �� �ϴµ� �ּҰ� ���� Node �� �Ҵ��
				// th

				// UINT64 index = InterlockedIncrement64(&logIndex);
				// logging[index % LOG_MAX] = { index, GetCurrentThreadId(), DEQUEUE, 3, readHead, next, readTail, readTailNext };
				// 
				// // tail->next�� NULL�� �ƴ� ���
				// if (readTailNext != NULL)
				// 	__debugbreak();

				continue;
				// �� ������ ��¥ head->next NULL���� Ȯ��
				// �׸��� ��¼�� head->next NULL�� �Ǿ������� Ȯ��

				// UINT64 index = InterlockedIncrement64(&logIndex);
				// logging[index % LOG_MAX] = { index, GetCurrentThreadId(), DEQUEUE, 3, readHead, next, readTail, readTailNext };
				// __debugbreak();
				return false;
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

					Node *nextAddr = (Node *)GetAddress(next);
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

