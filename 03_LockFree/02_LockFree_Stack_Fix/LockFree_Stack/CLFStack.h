#pragma once

struct DebugInfo
{
	LONG64		index;
	DWORD		threadId;
	USHORT		pushOrPop;		// push : 0							
								// pop : 1
	USHORT		data;			// ������
	void		*tPtr;			// �о�� Top
	void		*newTPtr;		// ���� Top �Ǿ���� ��
};

#define LOG_MAX 200000
#define PUSH 0
#define POP 1

extern DebugInfo logging[LOG_MAX];
extern LONG64 logIndex;

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
		Node *newTop = m_StackNodePool.Alloc();
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

	void Pop(T *data)
	{
		ULONG_PTR readTop;
		ULONG_PTR newTop;
		Node *readTopAddr;

		do
		{
			readTop = m_pTop;
			readTopAddr = (Node *)GetAddress(readTop);
			newTop = readTopAddr->next;
		} while (InterlockedCompareExchange(&m_pTop, newTop, readTop) != readTop);

		// Pop ����
		InterlockedDecrement(&m_iUseCount);

		*data = readTopAddr->data;
		m_StackNodePool.Free(readTopAddr);
	}

public:
	ULONG_PTR		m_pTop = NULL;
	ULONG_PTR		m_ullCurrentIdentifier = 0; // ABA ������ �ذ��ϱ� ���� �ĺ���
	CLFMemoryPool<Node> m_StackNodePool = CLFMemoryPool<Node>(1000, false);

	LONG			m_iUseCount = 0;
};

