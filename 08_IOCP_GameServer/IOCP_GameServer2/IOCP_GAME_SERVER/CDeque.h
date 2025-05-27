#pragma once

template<typename T>
class CDeque
{
public:
	struct DequeNode
	{
		DequeNode() noexcept = default;
		DequeNode(T data) noexcept : tData(data), prev(nullptr), next(nullptr) {}

		T tData;
		DequeNode *prev;
		DequeNode *next;
	};

	class iterator
	{
	public:
		iterator(DequeNode *node = nullptr) noexcept : m_Node(node) {}
		iterator operator++(int) noexcept
		{
			DequeNode *tempNode = m_Node;
			m_Node = m_Node->next;
			return iterator(tempNode);
		}

		iterator &operator++() noexcept
		{
			m_Node = m_Node->next;
			return *this;
		}

		iterator operator--(int) noexcept
		{
			DequeNode *tempNode = m_Node;
			m_Node = m_Node->prev;
			return iterator(tempNode);
		}

		iterator &operator--() noexcept
		{
			m_Node = m_Node->prev;
			return *this;
		}

		T &operator*() noexcept
		{
			return m_Node->tData;
		}

		bool operator==(const iterator &other) noexcept
		{
			return m_Node == other.m_Node;
		}

		bool operator!=(const iterator &other) noexcept
		{
			return !operator==(other);
		}

		friend iterator CDeque::erase(iterator) noexcept;
	private:
		operator DequeNode *() noexcept
		{
			return m_Node;
		}

		DequeNode *m_Node;
	};

	CDeque() noexcept
	{
		m_Head.next = &m_Tail;
		m_Head.prev = nullptr;

		m_Tail.next = nullptr;
		m_Tail.prev = &m_Head;
	}

	~CDeque() noexcept
	{
		clear();
	}

	T back() noexcept { return m_Tail.prev->tData; }
	T front() { return m_Head.next->tData; }

	void push_back(T data) noexcept;
	void push_front(T data) noexcept;
	void pop_back() noexcept;
	void pop_front() noexcept;
	void clear() noexcept;
	bool empty() noexcept { return m_Size == 0; }

	iterator begin() noexcept { return iterator(m_Head.next); }
	iterator end() noexcept { return iterator(&m_Tail); }
	int size() noexcept { return m_Size; }

	iterator erase(iterator it) noexcept
	{
		iterator delIt = it++;
		delete_node(delIt);
		return it;
	}

	void remove(T data) noexcept
	{
		for (CDeque::iterator it = begin(); it != end();)
		{
			if (*it == data)
			{
				it = erase(it);
			}
			{
				++it;
			}
		}
	}
private:
	// 항상 target의 next에만 삽입
	void append_node(DequeNode *target, T data) noexcept;

	// delNode를 삭제
	void delete_node(DequeNode *delNode) noexcept;

private:
	int			m_Size = 0;
	DequeNode		m_Head;
	DequeNode		m_Tail;

	// inline static CSingleMemoryPool<DequeNode> s_NodePool = CSingleMemoryPool<DequeNode>();
};

template<typename T>
inline void CDeque<T>::push_back(T data) noexcept
{
	DequeNode *target = m_Tail.prev;
	append_node(target, data);
}

template<typename T>
inline void CDeque<T>::push_front(T data) noexcept
{
	DequeNode *target = &m_Head;
	append_node(target, data);
}

template<typename T>
inline void CDeque<T>::pop_back() noexcept
{
	DequeNode *delNode = m_Tail.prev;
	delete_node(delNode);
}

template<typename T>
inline void CDeque<T>::pop_front() noexcept
{
	DequeNode *delNode = m_Head.next;
	delete_node(delNode);
}

template<typename T>
inline void CDeque<T>::clear() noexcept
{
	DequeNode *delNode = m_Head.next;
	while (delNode != &m_Tail)
	{
		DequeNode *nextDelNode = delNode->next;
		delete delNode;
		delNode = nextDelNode;
	}

	m_Size = 0;
	m_Head.next = &m_Tail;
	m_Tail.prev = &m_Head;
}

template<typename T>
inline void CDeque<T>::append_node(DequeNode *target, T data) noexcept
{
	DequeNode *newNode = new DequeNode;
	newNode->tData = data;
	DequeNode *nextNode = target->next;
	DequeNode *prevNode = target;
	newNode->next = nextNode;
	newNode->prev = prevNode;

	nextNode->prev = newNode;
	prevNode->next = newNode;

	m_Size++;
}

template<typename T>
inline void CDeque<T>::delete_node(DequeNode *delNode) noexcept
{
	DequeNode *delNodeNext = delNode->next;
	DequeNode *delNodePrev = delNode->prev;

	delNodeNext->prev = delNodePrev;
	delNodePrev->next = delNodeNext;

	delete delNode;
	m_Size--;
}