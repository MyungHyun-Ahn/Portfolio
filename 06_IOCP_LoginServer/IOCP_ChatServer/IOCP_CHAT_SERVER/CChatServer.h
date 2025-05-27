#pragma once

class CChatProcessPacketInterface;

class CChatServer : public NET_SERVER::CNetServer
{
public:
	friend class CChatProcessPacket;
	friend class CMonitor;

	CChatServer() noexcept;

	void		Update() noexcept;
	void		NonLoginHeartBeat() noexcept;
	void		LoginHeartBeat() noexcept;

	void		SendSector(UINT64 sessionId, WORD sectorY, WORD sectorX, CSmartPtr<CSerializableBuffer<SERVER_TYPE::WAN>> message) noexcept;

	// CNetServer을(를) 통해 상속됨
	bool		OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept override;
	void		OnAccept(const UINT64 sessionID) noexcept override;
	void		OnClientLeave(const UINT64 sessionID) noexcept override;
	void		OnRecv(const UINT64 sessionID, CSerializableBufferView<SERVER_TYPE::WAN> *message) noexcept override;
	void		OnRecv(const UINT64 sessionID, CSmartPtr<CSerializableBufferView<SERVER_TYPE::WAN>> message) noexcept override;
	void		OnError(int errorcode, WCHAR *errMsg) noexcept override;

	DWORD		OnUpdate() noexcept override;

	void		SectorBroadcast() noexcept;

	void		RegisterContentTimerEvent() noexcept override;

	inline int GetPlayerCount() noexcept { return (int)m_umapLoginPlayer.size(); }
	inline int GetJobQCount() noexcept { return m_RecvJobQ.GetUseSize(); }

private:
	std::unordered_map<UINT64, CNonLoginPlayer>		m_umapNonLoginPlayer;
	std::unordered_map<UINT64, CPlayer *>			m_umapLoginPlayer;
	CChatProcessPacketInterface						*m_pProcessPacket = nullptr;
	CLFQueue<CSerializableBufferView<SERVER_TYPE::WAN> *>		m_RecvJobQ;
	CSector											m_arrCSector[MAX_SECTOR_Y][MAX_SECTOR_X];
};