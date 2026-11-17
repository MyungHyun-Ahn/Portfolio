#pragma once
// #define SAFE_MODE // 메모리 풀에서 할당한 것인지 판별 용도

namespace MHLib::memory
{
	template<typename TData>
	class CSingleMemoryPool
	{
	public:
		struct SNode
		{
#ifdef SAFE_MODE
			void *m_pPool;
#endif
			TData m_data;
			SNode *m_pLink;

#ifdef SAFE_MODE
			int m_safeWord = 0xFFFF;
#endif
		};

	public:
		__forceinline CSingleMemoryPool(int initCount = 0) noexcept
			: m_useCount(0)
			, m_capacity(0)
		{
			// initCount 만큼 FreeList 할당
			for (int i = 0; i < initCount; i++)
			{
				SNode *pNode = newAlloc();
				pNode->m_pLink = m_pTop;
				m_pTop = pNode;
			}
		}

		__forceinline TData *Alloc() noexcept
		{
			SNode *pNode;

			if (m_pTop == nullptr)
			{
				pNode = newAlloc();
			}
			else
			{
				pNode = m_pTop;
				m_pTop = m_pTop->m_pLink;
			}

			m_useCount++;
			// data의 주소를 반환
			return &pNode->m_data;
		}
		__forceinline void Free(TData *pPtr) noexcept
		{
#ifdef SAFE_MODE
			unsigned __int64 intPtr = (__int64)pPtr;
			intPtr -= sizeof(void *);
			SNode *pNodePtr = (SNode *)intPtr;

			if (pNodePtr->m_pPool != this)
				throw;

			if (pNodePtr->m_safeWord != 0xFFFF)
				throw;
#else
			SNode *pNodePtr = (SNode *)pPtr;
#endif
			pNodePtr->m_pLink = m_pTop;
			m_pTop = pNodePtr;

			m_useCount--;
		}

		__forceinline LONG GetCapacity() noexcept { return m_capacity; }
		__forceinline LONG GetUseCount() noexcept { return m_useCount; }

	private:
		__forceinline SNode *newAlloc() noexcept
		{
			SNode *pNewNode = new SNode;
#ifdef SAFE_MODE
			pNewNode->m_pPool = this;
#endif
			m_capacity++;
			return pNewNode;
		}

	private:
		// FreeList
		int m_useCount = 0;
		int m_capacity = 0;
		SNode *m_pTop = nullptr;
	};
}