#pragma once

// 스마트 포인터를 사용할 클래스는
// 내부적으로 인터락을 사용하여 레퍼런스 카운트를 관리하는 인터페이스 제공
//   * LONG IncreaseRef();
//   * LONG DecreaseRef();
// T::Free() 인터페이스를 제공
// 네트워크 라이브러리에서는 위 IncreaseRef(), DecreaseRef() 함수를 통해 직접 레퍼런스 카운트의 제어를 수행
//   * 컨텐츠 코드에서는 아래의 CSmartPtr 사용 

#include <windows.h>
#include <concepts>

namespace MHLib::utils
{
	template<typename T>
	concept SmartPtrCompatible = requires(T t)
	{
		{ t.IncreaseRef() } -> std::same_as<LONG>;
		{ t.DecreaseRef() } -> std::same_as<LONG>;
		{ T::Free(std::declval<T *>()) } -> std::same_as<void>;
	};

	template<SmartPtrCompatible T>
	class CSmartPtr
	{
	public:
		CSmartPtr() noexcept = default;

		// 생성 시 증가
		CSmartPtr(T *ptr) noexcept
		{
			m_ptr = ptr;
			m_ptr->IncreaseRef();
		}

		// 명시적 호출해도 됨
		~CSmartPtr() noexcept
		{
			// 0이 되면
			if (m_ptr != nullptr && m_ptr->DecreaseRef() == 0)
				T::Free(m_ptr);

			m_ptr = nullptr;
		}

		CSmartPtr(const CSmartPtr &sPtr) noexcept
		{
			m_ptr = sPtr.m_ptr;
			m_ptr->IncreaseRef();
		}


		CSmartPtr &operator=(const CSmartPtr &sPtr) noexcept
		{
			m_ptr = sPtr.m_ptr;
			m_ptr->IncreaseRef();
			return *this;
		}

		T &operator*() noexcept
		{
			return *m_ptr;
		}

		T *operator->() noexcept
		{
			return m_ptr;
		}

		inline static CSmartPtr MakeShared()  noexcept
		{
			T *ptr = T::Alloc();
			return CSmartPtr(ptr);
		}

	public:
		inline T *GetRealPtr() const noexcept { return m_ptr; }

	private:
		T *m_ptr = nullptr;
	};
}