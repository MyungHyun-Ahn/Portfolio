#pragma once

struct StackDebug
{
	LONG64		index;
	DWORD		threadId;
	USHORT		pushOrPop;		// push : 0							
								// pop : 1
	USHORT		data;			// ������
	void *tPtr;			// �о�� Top
	void *newTPtr;		// ���� Top �Ǿ���� ��
};

#define USE_LFSTACK_DEBUG

#define LOG_MAX 200000
#define PUSH 0
#define POP 1

extern StackDebug StackLogging[LOG_MAX];
extern LONG64 StackLogIndex;

template<typename Data>
struct StackNode
{
	Data data;
	ULONG_PTR next;
};

template<typename T>
class CLFStack
{
public:
	using Node = StackNode<T>;

	void Push(T data)
	{
		// ���� Push �� ���� �ĺ��� �߱�
		ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
		ULONG_PTR readTop;
		Node *newTop = s_StackNodePool.Alloc();
		newTop->data = data;
		ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)newTop);

		do
		{
			// readTop ���� ident�� �ռ��� �ּ�
			readTop = m_pTop;
			newTop->next = readTop;
		} while (InterlockedCompareExchange(&m_pTop, combinedNewTop, readTop) != readTop);

		// Push ����
		InterlockedIncrement(&m_iUseCount);
	}

	bool Pop(T *data)
	{
		ULONG_PTR readTop;
		ULONG_PTR newTop;
		Node *readTopAddr;

		do
		{
			readTop = m_pTop;
			readTopAddr = (Node *)GetAddress(readTop);
			if (readTop == NULL)
			{
				break;
			}
			newTop = readTopAddr->next;
		} while (InterlockedCompareExchange(&m_pTop, newTop, readTop) != readTop);

		if (readTopAddr == NULL)
			return false;

		// Pop ����
		InterlockedDecrement(&m_iUseCount);

		*data = readTopAddr->data;
		s_StackNodePool.Free(readTopAddr);
		return true;
	}

public:
	ULONG_PTR				m_pTop = NULL;
	ULONG_PTR				m_ullCurrentIdentifier = 0; // ABA ������ �ذ��ϱ� ���� �ĺ���
	inline static CLFMemoryPool<Node>		s_StackNodePool = CLFMemoryPool<Node>(0, false);

	LONG					m_iUseCount = 0;
};

