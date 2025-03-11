#pragma once
class CLanMonitoringServer : public LAN_SERVER::CLanServer
{
public:
	CLanMonitoringServer();

	// CLanServer��(��) ���� ��ӵ�
	bool OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept override;
	void OnAccept(const UINT64 sessionID) noexcept override;
	void OnClientLeave(const UINT64 sessionID) noexcept override;
	void OnRecv(const UINT64 sessionID, CSerializableBufferView<TRUE> *message) noexcept override;
	void OnError(int errorcode, WCHAR *errMsg) noexcept override;
	void RegisterContentTimerEvent() noexcept override;

private:
	// INT�� 0�̸� ���� �α��ε��� ���� ����
	std::unordered_map<UINT64, INT> m_umapServers;
	SRWLOCK m_umapLock;
};

