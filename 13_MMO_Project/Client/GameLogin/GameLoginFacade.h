#pragma once

namespace GameLoginBridge
{
	enum class Phase { Idle, Connecting, Waiting, Rejected, Failed, Accepted };

	// 모든 호출과 해제는 Tick을 소유한 앱 스레드에서 한다.
	class ILogin
	{
	public:
		virtual ~ILogin() = default;
		virtual bool Submit(const wchar_t* pId) noexcept = 0;
		virtual void Tick() noexcept = 0;
		virtual Phase GetPhase() const noexcept = 0;
		virtual unsigned long long GetCharacterId() const noexcept = 0;
		virtual unsigned int GetLastLogoutMap() const noexcept = 0;
	};

	[[nodiscard]]
	ILogin* Create(unsigned short port) noexcept;
	void Destroy(ILogin* pLogin) noexcept;
}
