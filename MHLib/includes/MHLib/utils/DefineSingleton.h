#pragma once

namespace MHLib::utils
{
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
				atexit(Destroy);
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
}