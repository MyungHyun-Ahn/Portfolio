#pragma once

// ����Ʈ �����͸� ����� Ŭ������
// ���������� ���Ͷ��� ����Ͽ� ���۷��� ī��Ʈ�� �����ϴ� �������̽� ����
//   * LONG IncreaseRef();
//   * LONG DecreaseRef();
// T::Free() �������̽��� ����
// ��Ʈ��ũ ���̺귯�������� �� IncreaseRef(), DecreaseRef() �Լ��� ���� ���� ���۷��� ī��Ʈ�� ��� ����
//   * ������ �ڵ忡���� �Ʒ��� CSmartPtr ��� 

template<typename T>
class CSmartPtr
{
public:
	CSmartPtr() = default;

	// ���� �� ����
	CSmartPtr(T *ptr)
	{
		m_ptr = ptr;
		m_ptr->IncreaseRef();
	}

	// ����� ȣ���ص� ��
	~CSmartPtr()
	{
		// 0�� �Ǹ�
		if (m_ptr != nullptr && m_ptr->DecreaseRef() == 0)
			T::Free(m_ptr);

		m_ptr = nullptr;
	}

	CSmartPtr(const CSmartPtr &sPtr)
	{
		m_ptr = sPtr.m_ptr;
		m_ptr->IncreaseRef();
	}


	CSmartPtr &operator=(const CSmartPtr &sPtr)
	{
		m_ptr = sPtr.m_ptr;
		m_ptr->IncreaseRef();
		return *this;
	}

	T &operator*()
	{
		return *m_ptr;
	}

	T *operator->()
	{
		return m_ptr;
	}

	inline static CSmartPtr MakeShared() 
	{
		T* ptr = T::Alloc();
		return CSmartPtr(ptr);
	}

public:
	inline T *GetRealPtr() { return m_ptr; }

private:
	T *m_ptr = nullptr;
};

