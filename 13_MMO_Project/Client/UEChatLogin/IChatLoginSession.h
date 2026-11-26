#pragma once

namespace Chatting
{
	struct SLoginTickResult
	{
		bool m_submitted = false;
		long long m_acceptedAccount = 0;
		bool m_disconnected = false;
		unsigned long m_error = 0;
	};

	class IChatLoginSession
	{
	public:
		virtual ~IChatLoginSession() = default;
		virtual SLoginTickResult TickLogin() noexcept = 0;
	};

	[[nodiscard]]
	IChatLoginSession* StartLocalLogin(unsigned short port, long long account) noexcept;
}
