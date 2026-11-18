#pragma once

namespace Chatting
{
	class CChatContents;
	class CProcessPacket;

	class CPlayer
	{
	public:
		friend class CChatContents;
		friend class CProcessPacket;

		void Clear() noexcept
		{
			m_sectorY = 0xff;
			m_sectorX = 0xff;
			m_accountNo = 0;
			m_prevRecvTime = timeGetTime();
			ZeroMemory(m_id, sizeof(m_id));
			ZeroMemory(m_nickname, sizeof(m_nickname));
		}

		static CPlayer *Alloc() noexcept
		{
			CPlayer *pPlayer = sm_playerPool.Alloc();
			pPlayer->Clear();
			return pPlayer;
		}

		static void Free(CPlayer *pPlayer) noexcept
		{
			sm_playerPool.Free(pPlayer);
		}

	private:
		INT64 m_accountNo;
		DWORD m_prevRecvTime;
		WORD m_sectorY;
		WORD m_sectorX;
		WCHAR m_id[kNameLength];
		WCHAR m_nickname[kNameLength];
		inline static MHLib::memory::CTlsMemoryPoolManager<CPlayer> sm_playerPool;
	};
}
