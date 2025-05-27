#pragma once
class CMonitorClient : public LAN_CLIENT::CLanClient
{
	// CLanClient을(를) 통해 상속됨
	void OnConnect(const UINT64 sessionID) noexcept override;
	void OnDisconnect(const UINT64 sessionID) noexcept override;
	void OnRecv(const UINT64 sessionID, CSerializableBufferView<SERVER_TYPE::LAN> *message) noexcept override;
	void RegisterContentTimerEvent() noexcept override;
};

extern CMonitorClient *g_MonitorClient;
