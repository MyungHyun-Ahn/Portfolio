#pragma once

class CChatProcessPacketInterface;

class CChatServer : public CNetServer
{
public:
	friend class CChatProcessPacket;

	CChatServer() noexcept;

	void		NonLoginHeartBeat() noexcept;
	void		LoginHeartBeat() noexcept;
	void		SendSector(UINT64 sessionId, WORD sectorY, WORD sectorX, CSerializableBuffer<FALSE> *message) noexcept;
	void		SectorBroadcast() noexcept;

	// CNetServer을(를) 통해 상속됨
	bool		OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept override;
	void		OnAccept(const UINT64 sessionID) noexcept override;
	void		OnClientLeave(const UINT64 sessionID) noexcept override;
	void		OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept override;
	void		OnError(int errorcode, WCHAR *errMsg) noexcept override;

	void		RegisterContentTimerEvent() noexcept override; // 스케줄러 스레드가 생성된 이후 호출되어야 함


	inline int GetPlayerCount() noexcept { return (int)m_umapLoginPlayer.size(); }
	inline int GetJobQCount() noexcept { return m_RecvJobQ.GetUseSize(); }

private:

	std::unordered_map<UINT64, CNonLoginPlayer>		m_umapNonLoginPlayer;
	SRWLOCK											m_nonLoginplayerMapLock;
	std::unordered_map<UINT64, CPlayer *>			m_umapLoginPlayer;
	SRWLOCK											m_playerMapLock;

	CChatProcessPacketInterface						*m_pProcessPacket = nullptr;
	CLFQueue<CSerializableBufferView<FALSE> *>		m_RecvJobQ;
	CSector											m_arrCSector[MAX_SECTOR_Y][MAX_SECTOR_X];
};