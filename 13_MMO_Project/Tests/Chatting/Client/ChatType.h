#pragma once

namespace Chatting
{
	enum class ELoginState
	{
		Idle,
		Pending,
		Accepted
	};

	struct SSectorPosition
	{
		std::uint16_t m_x = 0;
		std::uint16_t m_y = 0;
	};
}