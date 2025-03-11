#pragma once

class CNetMonitoringServer : public NET_SERVER::CNetServer
{
public:
	CNetMonitoringServer();

	// CNetServer을(를) 통해 상속됨
	bool OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept override;
	void OnAccept(const UINT64 sessionID) noexcept override;
	void OnClientLeave(const UINT64 sessionID) noexcept override;
	void OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept override;
	void OnError(int errorcode, WCHAR *errMsg) noexcept override;
	void RegisterContentTimerEvent() noexcept override;

	// CLanMonitoringServer에서 호출
	void SendMonitoringData(BYTE serverNo, BYTE dataType, int dataValue, int timeStamp) noexcept;

private:
	// 모니터링 클라 BOOL 값은 로그인 여부
	std::unordered_map<UINT64, BOOL> m_umapMonitorClients;
	SRWLOCK m_umapLock;
};

