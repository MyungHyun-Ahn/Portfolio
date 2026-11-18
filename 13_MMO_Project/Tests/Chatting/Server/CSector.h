#pragma once

namespace Chatting
{
	class CPlayer;
	class CChatContents;
	class CProcessPacket;

	class CSector
	{
	public:
		friend class CChatContents;
		friend class CProcessPacket;

	private:
		// 같은 콘텐츠 프레임에서만 접근한다.
		std::unordered_map<UINT64, CPlayer *> m_players;
	};
}
