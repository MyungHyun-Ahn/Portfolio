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

	// CNetServer을(를) 통해 상속됨
	bool		OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept override;
	void		OnAccept(const UINT64 sessionID) noexcept override;
	void		OnClientLeave(const UINT64 sessionID) noexcept override;
	void		OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept override;
	void		OnRecv(const UINT64 sessionID, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept override;
	void		OnError(int errorcode, WCHAR *errMsg) noexcept override;
	void		OnHeartBeat() noexcept override;
	void		OnSectorBroadcast() noexcept override;

	inline int GetPlayerCount() noexcept { return (int)m_umapLoginPlayer.size(); }
	inline int GetJobQCount() noexcept { return m_RecvJobQ.GetUseSize(); }

private:
	// 락을 같은 걸 써야함
	// - nonLoginPlayer의 상태 혹은 login 상태의 플레이어 둘 중 하나에는 무조건 플레이어가 존재해야 함
	// - login 메시지 처리 도중 non login 에서 login으로 옮기는 도중 간격이 생김
	std::unordered_map<UINT64, CNonLoginPlayer>		m_umapNonLoginPlayer;
	std::unordered_map<UINT64, CPlayer *>			m_umapLoginPlayer;
	SRWLOCK											m_playerMapLock;

	CChatProcessPacketInterface						*m_pProcessPacket = nullptr;
	CLFQueue<CSerializableBufferView<FALSE> *>		m_RecvJobQ;
	CSector											m_arrCSector[MAX_SECTOR_Y][MAX_SECTOR_X];
};