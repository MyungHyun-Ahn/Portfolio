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
struct Node
{
	Data data;
	Node *next;
};

template<typename T>
class CLFStack
{
public:
	void Push(T data)
	{
		Node<T> *newTop = new Node<T>;
		newTop->data = data;
		Node<T> *t;
		do
		{
			t = m_pTop;
			newTop->next = t;
		} while (InterlockedCompareExchangePointer((volatile LPVOID *)&m_pTop, (LPVOID)newTop, (LPVOID)t) != t);

		LONG64 backIndex = InterlockedIncrement64(&logIndex);
		logging[backIndex % LOG_MAX] = { backIndex, GetCurrentThreadId(), PUSH, (USHORT)data, (LPVOID)t, (LPVOID)newTop };
		// Push ����
		// InterlockedIncrement(&m_iUseCount);
	}

	void Pop(T *data)
	{
		Node<T> *t;
		Node<T> *newTop;

		do
		{
			t = m_pTop;
			newTop = t->next;
		} while (InterlockedCompareExchangePointer((volatile LPVOID *)&m_pTop, (LPVOID)newTop, (LPVOID)t) != t);

		LONG64 backIndex = InterlockedIncrement64(&logIndex);
		logging[backIndex % LOG_MAX] = { backIndex, GetCurrentThreadId(), POP, (USHORT)t->data, (LPVOID)t, (LPVOID)newTop };

		// Pop ����
		// InterlockedDecrement(&m_iUseCount);

		*data = t->data;
		delete t;
	}

public:
	Node<T>			*m_pTop = nullptr;
	LONG			m_iUseCount = 0;
};

