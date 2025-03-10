#pragma once
class CGameServer : public NETWORK_SERVER::CNetServer
{
public:
	friend class CAuthContent;
	friend class CEchoContent;
	friend class CMonitor;

	CGameServer();

	// CNetServer을(를) 통해 상속됨
	bool OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept override;
	void OnAccept(const UINT64 sessionID) noexcept override;
	void OnClientLeave(const UINT64 sessionID) noexcept override;
	void OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept override;
	void OnError(int errorcode, WCHAR *errMsg) noexcept override;
	void RegisterContentTimerEvent() noexcept override;

private:
	CAuthContent *m_pAuthContent;
	CEchoContent *m_pEchoContent;
};

