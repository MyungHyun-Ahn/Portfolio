#pragma once

enum class LOCK_TYPE
{
    EXCLUSIVE,
    SHARED
};

template<LOCK_TYPE lockType>
class CLockGuard
{
public:
    // ������: SRWLOCK ��ü�� �޾Ƽ� ����� ȹ��
    explicit CLockGuard(SRWLOCK &lock) noexcept
        : m_lock(lock)
    {
        if constexpr (lockType)
        {
            AcquireSRWLockExclusive(&m_lock); // ���� ���
        }
        else
        {
            AcquireSRWLockShared(&m_lock); // �б� ���
        }
    }

    // �Ҹ���: SRWLOCK ����� ����
    ~CLockGuard() noexcept
    {
        if constexpr (lockType)
        {
            ReleaseSRWLockExclusive(&m_lock); // ���� ��� ����
        }
        else
        {
            ReleaseSRWLockShared(&m_lock); // �б� ��� ����
        }
    }

    // ���� �����ڿ� ���� ���� �����ڸ� �����Ͽ� ���� ����
    CLockGuard(const CLockGuard &) = delete;
    CLockGuard &operator=(const CLockGuard &) = delete;

private:
    SRWLOCK &m_lock; // ������ SRWLOCK ��ü�� ����
};