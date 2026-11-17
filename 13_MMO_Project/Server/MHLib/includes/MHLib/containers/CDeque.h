#pragma once
namespace MHLib::containers
{
	template<typename TObject>
	class CDeque
	{
	public:
		struct SDequeNode
		{
			SDequeNode() noexcept = default;
			SDequeNode(TObject data) noexcept
				: m_data(data)
				, m_pPrev(nullptr)
				, m_pNext(nullptr)
			{
			}

			TObject m_data;
			SDequeNode *m_pPrev;
			SDequeNode *m_pNext;
		};

		class CIterator
		{
		public:
			CIterator(SDequeNode *pNode = nullptr) noexcept : m_pNode(pNode) {}
			CIterator operator++(int) noexcept
			{
				SDequeNode *pTempNode = m_pNode;
				m_pNode = m_pNode->m_pNext;
				return CIterator(pTempNode);
			}

			CIterator &operator++() noexcept
			{
				m_pNode = m_pNode->m_pNext;
				return *this;
			}

			CIterator operator--(int) noexcept
			{
				SDequeNode *pTempNode = m_pNode;
				m_pNode = m_pNode->m_pPrev;
				return CIterator(pTempNode);
			}

			CIterator &operator--() noexcept
			{
				m_pNode = m_pNode->m_pPrev;
				return *this;
			}

			TObject &operator*() noexcept
			{
				return m_pNode->m_data;
			}

			bool operator==(const CIterator &other) noexcept
			{
				return m_pNode == other.m_pNode;
			}

			bool operator!=(const CIterator &other) noexcept
			{
				return !operator==(other);
			}

			friend CIterator CDeque::Erase(CIterator) noexcept;
		private:
			operator SDequeNode *() noexcept
			{
				return m_pNode;
			}

			SDequeNode *m_pNode;
		};

		CDeque() noexcept
		{
			m_head.m_pNext = &m_tail;
			m_head.m_pPrev = nullptr;

			m_tail.m_pNext = nullptr;
			m_tail.m_pPrev = &m_head;
		}

		~CDeque() noexcept
		{
			Clear();
		}

		TObject Back() noexcept { return m_tail.m_pPrev->m_data; }
		TObject Front() { return m_head.m_pNext->m_data; }

		void PushBack(TObject data) noexcept;
		void PushFront(TObject data) noexcept;
		void PopBack() noexcept;
		void PopFront() noexcept;
		void Clear() noexcept;
		bool Empty() noexcept { return m_size == 0; }

		CIterator Begin() noexcept { return CIterator(m_head.m_pNext); }
		CIterator End() noexcept { return CIterator(&m_tail); }
		int Size() noexcept { return m_size; }

		CIterator Erase(CIterator it) noexcept
		{
			CIterator delIt = it++;
			deleteNode(delIt);
			return it;
		}

		void Remove(TObject data) noexcept
		{
			for (CDeque::CIterator it = Begin(); it != End();)
			{
				if (*it == data)
				{
					it = Erase(it);
				}
				{
					++it;
				}
			}
		}
	private:
		// 항상 target의 next에만 삽입
		void appendNode(SDequeNode *pTarget, TObject data) noexcept;

		// delNode를 삭제
		void deleteNode(SDequeNode *pDelNode) noexcept;

	private:
		int			m_size = 0;
		SDequeNode		m_head;
		SDequeNode		m_tail;

		// inline static CSingleMemoryPool<DequeNode> s_NodePool = CSingleMemoryPool<DequeNode>();
	};

	template<typename TObject>
	inline void CDeque<TObject>::PushBack(TObject data) noexcept
	{
		SDequeNode *pTarget = m_tail.m_pPrev;
		appendNode(pTarget, data);
	}

	template<typename TObject>
	inline void CDeque<TObject>::PushFront(TObject data) noexcept
	{
		SDequeNode *pTarget = &m_head;
		appendNode(pTarget, data);
	}

	template<typename TObject>
	inline void CDeque<TObject>::PopBack() noexcept
	{
		SDequeNode *pDelNode = m_tail.m_pPrev;
		deleteNode(pDelNode);
	}

	template<typename TObject>
	inline void CDeque<TObject>::PopFront() noexcept
	{
		SDequeNode *pDelNode = m_head.m_pNext;
		deleteNode(pDelNode);
	}

	template<typename TObject>
	inline void CDeque<TObject>::Clear() noexcept
	{
		SDequeNode *pDelNode = m_head.m_pNext;
		while (pDelNode != &m_tail)
		{
			SDequeNode *pNextDelNode = pDelNode->m_pNext;
			delete pDelNode;
			pDelNode = pNextDelNode;
		}

		m_size = 0;
		m_head.m_pNext = &m_tail;
		m_tail.m_pPrev = &m_head;
	}

	template<typename TObject>
	inline void CDeque<TObject>::appendNode(SDequeNode *pTarget, TObject data) noexcept
	{
		SDequeNode *pNewNode = new SDequeNode;
		pNewNode->m_data = data;
		SDequeNode *pNextNode = pTarget->m_pNext;
		SDequeNode *pPrevNode = pTarget;
		pNewNode->m_pNext = pNextNode;
		pNewNode->m_pPrev = pPrevNode;

		pNextNode->m_pPrev = pNewNode;
		pPrevNode->m_pNext = pNewNode;

		m_size++;
	}

	template<typename TObject>
	inline void CDeque<TObject>::deleteNode(SDequeNode *pDelNode) noexcept
	{
		SDequeNode *pDelNodeNext = pDelNode->m_pNext;
		SDequeNode *pDelNodePrev = pDelNode->m_pPrev;

		pDelNodeNext->m_pPrev = pDelNodePrev;
		pDelNodePrev->m_pNext = pDelNodeNext;

		delete pDelNode;
		m_size--;
	}
}