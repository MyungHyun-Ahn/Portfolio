#pragma once

#define MAX_SECTOR_Y 50
#define MAX_SECTOR_X 50

class CChatProcessPacketInterface;

class CChatServer : public CNetServer
{
public:
	friend class CChatProcessPacket;

	CChatServer() noexcept;

	void		Update() noexcept;
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

	// CNetServer을(를) 통해 상속됨
	DWORD		OnUpdate() noexcept override;
	void		OnHeartBeat() noexcept override;

	void		DelaySendSector() noexcept;

	inline int GetPlayerCount() { return m_umapLoginPlayer.size(); }
	inline int GetJobQCount() { return m_RecvJobQ.GetUseSize(); }

private:
	std::unordered_map<UINT64, CNonLoginPlayer>		m_umapNonLoginPlayer;
	std::unordered_map<UINT64, CPlayer *>			m_umapLoginPlayer;
	CChatProcessPacketInterface						*m_pProcessPacket = nullptr;
	CLFQueue<CSerializableBufferView<FALSE> *>		m_RecvJobQ;
	CSector											m_arrCSector[MAX_SECTOR_Y][MAX_SECTOR_X];
};