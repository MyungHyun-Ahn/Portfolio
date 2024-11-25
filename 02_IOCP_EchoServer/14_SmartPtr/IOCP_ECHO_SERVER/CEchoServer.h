#pragma once
class CEchoServer : public CLanServer
{
public:
	bool OnConnectionRequest(const WCHAR *ip, USHORT port);
	void OnAccept(const UINT64 sessionID); // Accept �� ���� ó�� �Ϸ� �� ȣ��
	void OnClientLeave(const UINT64 sessionID); // Release �� ȣ��
	void OnRecv(const UINT64 sessionID, CSerializableBufferView *message); // ��Ŷ ���� �Ϸ� ��
	void OnRecv(const UINT64 sessionID, CSmartPtr<CSerializableBufferView> message);
	void OnError(int errorcode, WCHAR *errMsg);
};

