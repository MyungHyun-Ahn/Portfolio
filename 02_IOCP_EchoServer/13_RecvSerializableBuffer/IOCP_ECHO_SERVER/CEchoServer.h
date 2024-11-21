#pragma once
class CEchoServer : public CLanServer
{
public:
	bool OnConnectionRequest(const WCHAR *ip, USHORT port);
	void OnAccept(const UINT64 sessionID); // Accept 후 접속 처리 완료 후 호출
	void OnClientLeave(const UINT64 sessionID); // Release 후 호출
	void OnRecv(const UINT64 sessionID, CSerializableBufferView *message); // 패킷 수신 완료 후
	void OnError(int errorcode, WCHAR *errMsg);
};

