#pragma once
/*
	Define.h
		* 프로젝트 전역적으로 사용될 것을 정의
*/

template <typename T>
class Singleton {
protected:
	Singleton() = default;
	virtual ~Singleton() = default;

public:
	Singleton(const Singleton &) = delete;
	Singleton &operator=(const Singleton &) = delete;

	static T *GetInstance() {
		if (m_instPtr == nullptr) {
			m_instPtr = new T();
			std::atexit(Destroy);
		}
		return m_instPtr;
	}

	static void Destroy() {
		delete m_instPtr;
		m_instPtr = nullptr;
	}

protected:
	inline static T *m_instPtr = nullptr;
};