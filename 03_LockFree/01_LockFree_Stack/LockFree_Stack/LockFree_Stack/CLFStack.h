#pragma once
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
		Node<T> *newNode = new Node<T>;
		newNode->data = data;
		Node<T> *t;
		do
		{
			t = m_pTop;
			newNode->next = t;
		} while (InterlockedCompareExchangePointer((volatile LPVOID *)&m_pTop, (LPVOID)newNode, (LPVOID)t) != t);

		// Push 성공
		InterlockedIncrement(&m_iUseCount);
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

		// Pop 성공
		InterlockedDecrement(&m_iUseCount);

		*data = t->data;
		delete t;
	}

public:
	Node<T>			*m_pTop = nullptr;
	LONG			m_iUseCount = 0;
};

