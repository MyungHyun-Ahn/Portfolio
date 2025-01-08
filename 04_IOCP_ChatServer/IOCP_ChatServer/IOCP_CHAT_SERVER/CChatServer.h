#pragma once
class CChatServer : public CNetServer
{
public:
	void Update() noexcept;
	void HeartBeat() noexcept;

	// CNetServer을(를) 통해 상속됨
	bool OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept override;
	void OnAccept(const UINT64 sessionID) noexcept override;
	void OnClientLeave(const UINT64 sessionID) noexcept override;
	void OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept override;
	void OnRecv(const UINT64 sessionID, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept override;
	void OnError(int errorcode, WCHAR *errMsg) noexcept override;

	// CNetServer을(를) 통해 상속됨
	DWORD OnUpdate() noexcept override;
	void OnHeartBeat() noexcept override;

private:
	// Player 관리 구조체 - 싱글에서 접근될 것이므로 동기화 X
};

