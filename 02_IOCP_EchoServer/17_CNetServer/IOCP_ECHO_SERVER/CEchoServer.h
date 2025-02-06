#pragma once
class CEchoServer : public CLanServer
{
public:
	bool OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept;
	void OnAccept(const UINT64 sessionID) noexcept; // Accept �� ���� ó�� �Ϸ� �� ȣ��
	void OnClientLeave(const UINT64 sessionID) noexcept; // Release �� ȣ��
	void OnRecv(const UINT64 sessionID, CSerializableBufferView<TRUE> *message) noexcept; // ��Ŷ ���� �Ϸ� ��
	void OnRecv(const UINT64 sessionID, CSmartPtr<CSerializableBufferView<TRUE>> message) noexcept;
	void OnError(int errorcode, WCHAR *errMsg) noexcept;
	void RegisterContentTimerEvent() noexcept override;
};

