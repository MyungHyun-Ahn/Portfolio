#pragma once

namespace Chatting
{
	class CChatServer;
	class CPlayer;
	class CProcessPacket;

	class CChatContents : public ServerNetworkLib::CBaseContents
	{
	public:
		friend class CProcessPacket;
		explicit CChatContents(CChatServer &server) noexcept;
		void OnEnter(UINT64 sessionId, void *pObject) noexcept override;
		void OnLeave(UINT64 sessionId) noexcept override;
		ServerNetworkLib::ERecvResult OnRecv(UINT64 sessionId, NetworkCommonLib::CSerializableBuffer *pMessage,
			int delayFrame) noexcept override;
		void OnLoopEnd() noexcept override;
		void SendSector(UINT64 sessionId, WORD sectorY, WORD sectorX, NetworkCommonLib::CSerializableBuffer *pMessage) noexcept;

	private:
		void nonLoginHeartbeat() noexcept;
		void loginHeartbeat() noexcept;
		CChatServer &m_server;
		CProcessPacket m_processPacket;
		std::unordered_map<UINT64, CPlayer *> m_umapNonLoginPlayer;
		std::unordered_map<UINT64, CPlayer *> m_umapLoginPlayer;
		CSector m_sectors[kMaxSectorY][kMaxSectorX];
		DWORD m_prevHeartbeatTime = 0;
	};
}
