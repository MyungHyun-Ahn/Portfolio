#pragma once

namespace Chatting
{
	class CChatServer;
	class CPlayer;
	class CProcessPacket;

	class CChatContents : public ContentsRuntimeLib::IContents
	{
	public:
		friend class CProcessPacket;
		explicit CChatContents(ContentsRuntimeLib::ContentsInstanceId instanceId, CChatServer &server) noexcept;
		void OnEnter(UINT64 sessionId) noexcept;
		void OnLeave(UINT64 sessionId) noexcept;
		void SendSector(UINT64 sessionId, WORD sectorY, WORD sectorX, const ServerNetworkLib::SendPacketPtr &spMessage) noexcept;

	protected:
		void OnFrame() noexcept override;

	private:
		bool onRecv(UINT64 sessionId, NetworkCommonLib::CSerializableBufferView &message) noexcept;
		void nonLoginHeartbeat() noexcept;
		void loginHeartbeat() noexcept;

	private:
		CChatServer &m_server;
		CProcessPacket m_processPacket;
		std::unordered_map<UINT64, CPlayer *> m_umapSessions;
		std::unordered_map<UINT64, CPlayer *> m_umapNonLoginPlayer;
		std::unordered_map<UINT64, CPlayer *> m_umapLoginPlayer;
		CSector m_sectors[kMaxSectorY][kMaxSectorX];
		DWORD m_prevHeartbeatTime = 0;
	};
}
