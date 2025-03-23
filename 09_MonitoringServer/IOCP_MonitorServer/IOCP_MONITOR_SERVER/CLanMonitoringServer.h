#pragma once

struct MonitoringInfo
{
	SRWLOCK srwLock;
	INT serverNo = 0;
	INT dataSum = 0;
	INT dataMax = INT_MIN;
	INT dataMin = INT_MAX;
	INT timeStamp = 0;
};

class CLanMonitoringServer : public LAN_SERVER::CLanServer
{
public:
	friend struct DBTimerEvent;

	CLanMonitoringServer();

	// CLanServer을(를) 통해 상속됨
	bool OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept override;
	void OnAccept(const UINT64 sessionID) noexcept override;
	void OnClientLeave(const UINT64 sessionID) noexcept override;
	void OnRecv(const UINT64 sessionID, CSerializableBufferView<TRUE> *message) noexcept override;
	void OnError(int errorcode, WCHAR *errMsg) noexcept override;
	void RegisterContentTimerEvent() noexcept override;

private:
	// INT가 0이면 아직 로그인되지 않은 서버
	std::unordered_map<UINT64, INT> m_umapServers;
	SRWLOCK m_umapLock;

	// MonitorInfo
	MonitoringInfo m_arrMonitorInfo[dfMONITOR_DATA_TYPE_END];
};

