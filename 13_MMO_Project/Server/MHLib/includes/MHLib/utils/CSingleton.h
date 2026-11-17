#pragma once

namespace MHLib::utils
{
	template <typename TObject>
	class CSingleton
	{
	protected:
		CSingleton() = default;
		virtual ~CSingleton() = default;

	public:
		CSingleton(const CSingleton &) = delete;
		CSingleton &operator=(const CSingleton &) = delete;

		static TObject *GetInstance()
		{
			if (sm_pInstance == nullptr)
			{
				sm_pInstance = new TObject();
				atexit(Destroy);
			}
			return sm_pInstance;
		}

		static void Destroy()
		{
			delete sm_pInstance;
			sm_pInstance = nullptr;
		}

	protected:
		inline static TObject *sm_pInstance = nullptr;
	};
}