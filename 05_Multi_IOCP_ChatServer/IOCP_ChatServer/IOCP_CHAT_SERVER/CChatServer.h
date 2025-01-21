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

	// CNetServer��(��) ���� ��ӵ�
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
	// ���� ���� �� �����
	// - nonLoginPlayer�� ���� Ȥ�� login ������ �÷��̾� �� �� �ϳ����� ������ �÷��̾ �����ؾ� ��
	// - login �޽��� ó�� ���� non login ���� login���� �ű�� ���� ������ ����
	std::unordered_map<UINT64, CNonLoginPlayer>		m_umapNonLoginPlayer;
	std::unordered_map<UINT64, CPlayer *>			m_umapLoginPlayer;
	SRWLOCK											m_playerMapLock;

	CChatProcessPacketInterface						*m_pProcessPacket = nullptr;
	CLFQueue<CSerializableBufferView<FALSE> *>		m_RecvJobQ;
	CSector											m_arrCSector[MAX_SECTOR_Y][MAX_SECTOR_X];
};