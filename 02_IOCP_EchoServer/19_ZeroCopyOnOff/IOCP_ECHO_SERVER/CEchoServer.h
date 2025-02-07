#pragma once
class CEchoServer : public CLanServer
{
public:
	bool OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept;
	void OnAccept(const UINT64 sessionID) noexcept; // Accept 후 접속 처리 완료 후 호출
	void OnClientLeave(const UINT64 sessionID) noexcept; // Release 후 호출
	void OnRecv(const UINT64 sessionID, CSerializableBufferView<TRUE> *message) noexcept; // 패킷 수신 완료 후
	void OnRecv(const UINT64 sessionID, CSmartPtr<CSerializableBufferView<TRUE>> message) noexcept;
	void OnError(int errorcode, WCHAR *errMsg) noexcept;
	void RegisterContentTimerEvent() noexcept override;
};

