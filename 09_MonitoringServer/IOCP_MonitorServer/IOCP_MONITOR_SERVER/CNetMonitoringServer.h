#pragma once

class CNetMonitoringServer : public NET_SERVER::CNetServer
{
public:
	CNetMonitoringServer();

	// CNetServer��(��) ���� ��ӵ�
	bool OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept override;
	void OnAccept(const UINT64 sessionID) noexcept override;
	void OnClientLeave(const UINT64 sessionID) noexcept override;
	void OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept override;
	void OnError(int errorcode, WCHAR *errMsg) noexcept override;
	void RegisterContentTimerEvent() noexcept override;

	// CLanMonitoringServer���� ȣ��
	void SendMonitoringData(BYTE serverNo, BYTE dataType, int dataValue, int timeStamp) noexcept;

private:
	// ����͸� Ŭ�� BOOL ���� �α��� ����
	std::unordered_map<UINT64, BOOL> m_umapMonitorClients;
	SRWLOCK m_umapLock;
};

