#pragma once

#include "../memory/CTLSMemoryPool.h"

namespace MHLib::containers
{
	using namespace MHLib::memory;

	template<typename Data>
	struct StackNode
	{
		Data data;
		ULONG_PTR next;
	};

	template<FundamentalOrPointer T, bool UseMemoryPool = TRUE>
	class CLFStack
	{
	};

	template<FundamentalOrPointer T>
	class CLFStack<T, TRUE>
	{
		static_assert(std::is_fundamental<T>::value || std::is_pointer<T>::value,
			"T must be a fundamental type or a pointer type.");

	public:
		using Node = StackNode<T>;

		void Push(T data) noexcept
		{
			// 새로 Push 할 때만 식별자 발급
			ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
			ULONG_PTR readTop;
			Node *newTop = s_StackNodePool.Alloc();
			newTop->data = data;
			ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)newTop);

			do
			{
				// readTop 또한 ident와 합성된 주소
				readTop = m_pTop;
				newTop->next = readTop;
			} while (InterlockedCompareExchange(&m_pTop, combinedNewTop, readTop) != readTop);

			// Push 성공
			InterlockedIncrement(&m_iUseCount);
		}

		bool Pop(T *data) noexcept
		{
			LONG size = InterlockedDecrement(&m_iUseCount);
			if (size < 0)
			{
				InterlockedIncrement(&m_iUseCount);
				return false;
			}


			ULONG_PTR readTop;
			ULONG_PTR newTop;
			Node *readTopAddr;

			do
			{
				readTop = m_pTop;
				readTopAddr = (Node *)GetAddress(readTop);
				newTop = readTopAddr->next;

			} while (InterlockedCompareExchange(&m_pTop, newTop, readTop) != readTop);

			*data = readTopAddr->data;
			s_StackNodePool.Free(readTopAddr);
			return true;
		}

	public:
		ULONG_PTR		m_pTop = NULL;
		ULONG_PTR		m_ullCurrentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
		LONG			m_iUseCount = 0;
		inline static CTLSMemoryPoolManager<Node> s_StackNodePool = CTLSMemoryPoolManager<Node>();
	};

	template<FundamentalOrPointer T>
	class CLFStack<T, FALSE>
	{
		static_assert(std::is_fundamental<T>::value || std::is_pointer<T>::value,
			"T must be a fundamental type or a pointer type.");

	public:
		using Node = StackNode<T>;

		void Push(T data) noexcept
		{
			// 새로 Push 할 때만 식별자 발급
			ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
			ULONG_PTR readTop;
			Node *newTop = new Node;
			newTop->data = data;
			ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)newTop);

			do
			{
				// readTop 또한 ident와 합성된 주소
				readTop = m_pTop;
				newTop->next = readTop;
			} while (InterlockedCompareExchange(&m_pTop, combinedNewTop, readTop) != readTop);

			// Push 성공
			InterlockedIncrement(&m_iUseCount);
		}

		void Pop(T *data)
		{
			LONG size = InterlockedDecrement(&m_iUseCount);
			if (size < 0)
			{
				InterlockedIncrement(&m_iUseCount);
				return false;
			}

			ULONG_PTR readTop;
			ULONG_PTR newTop;
			Node *readTopAddr;

			do
			{
				readTop = m_pTop;
				readTopAddr = (Node *)GetAddress(readTop);

				// 여기서 발생할 수 있는 예외는 오로지 decommit 된 페이지 참조 예외
				__try
				{
					newTop = readTopAddr->next;
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{
					continue;
				}

			} while (InterlockedCompareExchange(&m_pTop, newTop, readTop) != readTop);

			// Pop 성공
			InterlockedDecrement(&m_iUseCount);

			*data = readTopAddr->data;
			delete readTopAddr;
			return true;
		}

	public:
		ULONG_PTR		m_pTop = NULL;
		ULONG_PTR		m_ullCurrentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
		LONG			m_iUseCount = 0;
	};
}