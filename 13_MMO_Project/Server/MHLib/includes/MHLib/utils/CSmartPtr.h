#pragma once

// 스마트 포인터를 사용할 클래스는
// 내부적으로 인터락을 사용하여 레퍼런스 카운트를 관리하는 인터페이스 제공
//   * LONG IncreaseRef();
//   * LONG DecreaseRef();
// T::Free() 인터페이스를 제공
// 네트워크 라이브러리에서는 위 IncreaseRef(), DecreaseRef() 함수를 통해 직접 레퍼런스 카운트의 제어를 수행
//   * 컨텐츠 코드에서는 아래의 CSmartPtr 사용 

namespace MHLib::utils
{
	template<typename TObject>
	concept SmartPtrCompatible = requires(TObject t)
	{
		{ t.IncreaseRef() } -> std::same_as<LONG>;
		{ t.DecreaseRef() } -> std::same_as<LONG>;
		{ TObject::Free(std::declval<TObject *>()) } -> std::same_as<void>;
	};

	template<SmartPtrCompatible TObject>
	class CSmartPtr
	{
	public:
		CSmartPtr() noexcept = default;

		// 생성 시 증가
		CSmartPtr(TObject *pPtr) noexcept
		{
			m_pObject = pPtr;
			m_pObject->IncreaseRef();
		}

		// 명시적 호출해도 됨
		~CSmartPtr() noexcept
		{
			// 0이 되면
			if (m_pObject != nullptr && m_pObject->DecreaseRef() == 0)
				TObject::Free(m_pObject);

			m_pObject = nullptr;
		}

		CSmartPtr(const CSmartPtr &sPtr) noexcept
		{
			m_pObject = sPtr.m_pObject;
			m_pObject->IncreaseRef();
		}


		CSmartPtr &operator=(const CSmartPtr &sPtr) noexcept
		{
			m_pObject = sPtr.m_pObject;
			m_pObject->IncreaseRef();
			return *this;
		}

		TObject &operator*() noexcept
		{
			return *m_pObject;
		}

		TObject *operator->() noexcept
		{
			return m_pObject;
		}

		inline static CSmartPtr MakeShared()  noexcept
		{
			TObject *pPtr = TObject::Alloc();
			return CSmartPtr(pPtr);
		}

	public:
		inline TObject *GetRealPtr() const noexcept { return m_pObject; }

	private:
		TObject *m_pObject = nullptr;
	};
}