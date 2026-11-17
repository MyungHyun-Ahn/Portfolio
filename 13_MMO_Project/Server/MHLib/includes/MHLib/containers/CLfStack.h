#pragma once

namespace MHLib::containers
{
	using namespace MHLib::memory;

	template<typename TData>
	struct SStackNode
	{
		TData m_data;
		ULONG_PTR m_next;
	};

	template<FundamentalOrPointer TObject, bool useMemoryPool = TRUE>
	class CLfStack
	{
	};

	template<FundamentalOrPointer TObject>
	class CLfStack<TObject, TRUE>
	{
		static_assert(std::is_fundamental<TObject>::value || std::is_pointer<TObject>::value,
			"T must be a fundamental type or a pointer type.");

	public:
		using SNode = SStackNode<TObject>;

		void Push(TObject data) noexcept
		{
			// 새로 Push 할 때만 식별자 발급
			ULONG_PTR ident = InterlockedIncrement(&m_currentIdentifier);
			ULONG_PTR readTop;
			SNode *pNewTop = sm_stackNodePool.Alloc();
			pNewTop->m_data = data;
			ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)pNewTop);

			do
			{
				// readTop 또한 ident와 합성된 주소
				readTop = m_top;
				pNewTop->m_next = readTop;
			} while (InterlockedCompareExchange(&m_top, combinedNewTop, readTop) != readTop);

			// Push 성공
			InterlockedIncrement(&m_useCount);
		}

		bool Pop(TObject *outData) noexcept
		{
			LONG size = InterlockedDecrement(&m_useCount);
			if (size < 0)
			{
				InterlockedIncrement(&m_useCount);
				return false;
			}


			ULONG_PTR readTop;
			ULONG_PTR newTop;
			SNode *pReadTopAddr;

			do
			{
				readTop = m_top;
				pReadTopAddr = (SNode *)GetAddress(readTop);
				newTop = pReadTopAddr->m_next;

			} while (InterlockedCompareExchange(&m_top, newTop, readTop) != readTop);

			*outData = pReadTopAddr->m_data;
			sm_stackNodePool.Free(pReadTopAddr);
			return true;
		}

	public:
		ULONG_PTR		m_top = NULL;
		ULONG_PTR		m_currentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
		LONG			m_useCount = 0;
		inline static CTlsMemoryPoolManager<SNode> sm_stackNodePool = CTlsMemoryPoolManager<SNode>();
	};

	template<FundamentalOrPointer TObject>
	class CLfStack<TObject, FALSE>
	{
		static_assert(std::is_fundamental<TObject>::value || std::is_pointer<TObject>::value,
			"T must be a fundamental type or a pointer type.");

	public:
		using SNode = SStackNode<TObject>;

		void Push(TObject data) noexcept
		{
			// 새로 Push 할 때만 식별자 발급
			ULONG_PTR ident = InterlockedIncrement(&m_currentIdentifier);
			ULONG_PTR readTop;
			SNode *pNewTop = new SNode;
			pNewTop->m_data = data;
			ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)pNewTop);

			do
			{
				// readTop 또한 ident와 합성된 주소
				readTop = m_top;
				pNewTop->m_next = readTop;
			} while (InterlockedCompareExchange(&m_top, combinedNewTop, readTop) != readTop);

			// Push 성공
			InterlockedIncrement(&m_useCount);
		}

		bool Pop(TObject *outData)
		{
			LONG size = InterlockedDecrement(&m_useCount);
			if (size < 0)
			{
				InterlockedIncrement(&m_useCount);
				return false;
			}

			ULONG_PTR readTop;
			ULONG_PTR newTop;
			SNode *pReadTopAddr;

			do
			{
				readTop = m_top;
				pReadTopAddr = (SNode *)GetAddress(readTop);

				// 여기서 발생할 수 있는 예외는 오로지 decommit 된 페이지 참조 예외
				__try
				{
					newTop = pReadTopAddr->m_next;
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{
					continue;
				}

			} while (InterlockedCompareExchange(&m_top, newTop, readTop) != readTop);

			// Pop 성공
			InterlockedDecrement(&m_useCount);

			*outData = pReadTopAddr->m_data;
			delete pReadTopAddr;
			return true;
		}

	public:
		ULONG_PTR		m_top = NULL;
		ULONG_PTR		m_currentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
		LONG			m_useCount = 0;
	};
}