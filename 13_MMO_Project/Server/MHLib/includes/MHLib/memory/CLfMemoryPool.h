#pragma once

namespace MHLib::memory
{
	using namespace MHLib::containers;

#pragma warning(disable : 6001)
	// #define SAFE_MODE

	template<typename TData>
	struct SMemoryPoolNode
	{
#ifdef SAFE_MODE
		void *m_pPool;
#endif
		SMemoryPoolNode() noexcept = default;

		TData m_data;
		ULONG_PTR m_next;

#ifdef SAFE_MODE
		int m_safeWord = 0xFFFF;
#endif
	};

	// placement 지원 안함
	template<typename TData>
	class CLfMemoryPool
	{
		using SNode = SMemoryPoolNode<TData>;

	public:
		// 이 부분은 싱글 스레드에서 진행
		// - 다른 스레드가 생성되고는 절대 초기화를 진행하면 안됨
		__forceinline CLfMemoryPool(int initCount = 0) noexcept
			: m_capacity(0)
		{
			// initCount 만큼 FreeList 할당
			for (int i = 0; i < initCount; i++)
			{
				ULONG_PTR ident = InterlockedIncrement(&m_currentIdentifier);
				SNode *pNewTop = newAlloc();
				ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)pNewTop);
				pNewTop->m_next = m_top;
				m_top = combinedNewTop;
			}
		}

		// 다른 스레드가 모두 해제된 이후에 진행됨
		__forceinline ~CLfMemoryPool() noexcept
		{
			// FreeList에 있는 것 삭제
			while (m_top != NULL)
			{
				SNode *pDelNode = (SNode *)GetAddress(m_top);
				m_top = pDelNode->m_next;
				free(pDelNode);
			}

		}

		// Pop 행동
		TData *Alloc() noexcept
		{
			SNode *pNode;

			// nullptr을 보고 들어오면 그냥 할당하고 반환
			// - 이 시점에 Free 노드가 생겨도 그냥 할당해도 됨
			if (m_top == NULL)
			{
				pNode = newAlloc();
			}
			else
			{
				ULONG_PTR readTop;
				ULONG_PTR newTop;

				do
				{
					readTop = m_top;
					if (readTop == NULL)
					{
						pNode = newAlloc();
						break;
					}

					pNode = (SNode *)GetAddress(readTop);
					newTop = pNode->m_next;
				} while (InterlockedCompareExchange(&m_top, newTop, readTop) != readTop);
			}

			InterlockedIncrement(&m_useCount);

			// data의 주소를 반환
			return &pNode->m_data;
		}

		// 스택으로 치면 Push 행동
		void Free(TData *pPtr) noexcept
		{
#ifdef SAFE_MODE
			unsigned __int64 intPtr = (__int64)pPtr;
			intPtr -= sizeof(void *);
			SNode *pNewTop = (SNode *)intPtr;

			if (pNewTop->m_pPool != this)
				throw;

			if (pNewTop->m_safeWord != 0xFFFF)
				throw;
#else
			SNode *pNewTop = (SNode *)pPtr;
#endif
			ULONG_PTR ident = InterlockedIncrement(&m_currentIdentifier);
			ULONG_PTR readTop;
			ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)pNewTop);

			do
			{
				readTop = m_top;
				pNewTop->m_next = readTop;

			} while (InterlockedCompareExchange(&m_top, combinedNewTop, readTop) != readTop);

			// Push 성공
			InterlockedDecrement(&m_useCount);
		}

	private:
		__forceinline SNode *newAlloc() noexcept
		{
			SNode *pNewNode = new SNode;
#ifdef SAFE_MODE
			pNewNode->m_pPool = this;
#endif
			InterlockedIncrement(&m_capacity);
			return pNewNode;
		}

	public:
		inline LONG GetCapacity() const noexcept { return m_capacity; }
		inline LONG GetUseCount() const noexcept { return m_useCount; }

	private:
		LONG		m_useCount = 0;
		LONG		m_capacity = 0;
		ULONG_PTR	m_currentIdentifier = 0;
		ULONG_PTR	m_top = 0;
	};
}