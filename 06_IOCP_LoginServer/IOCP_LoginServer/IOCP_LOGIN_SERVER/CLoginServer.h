#pragma once

class CLoginProcessPacketInterface;

class CLoginServer : public NET_SERVER::CNetServer
{
public:
	friend class CLoginProcessPacket;

	CLoginServer() noexcept;

	void HeartBeat() noexcept;

	// CNetServer을(를) 통해 상속됨
	bool OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept override;
	void OnAccept(const UINT64 sessionID) noexcept override;
	void OnClientLeave(const UINT64 sessionID) noexcept override;
	void OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept override;
	void OnError(int errorcode, WCHAR *errMsg) noexcept override;
	void RegisterContentTimerEvent() noexcept override;

	inline int GetUserCount() noexcept { return (int)m_umapUser.size(); }

private:
	std::unordered_map<UINT64, CUser>		m_umapUser;
	SRWLOCK									m_userMapLock;

	CLoginProcessPacketInterface			*m_pProcessPacket = nullptr;
};

