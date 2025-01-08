#pragma once
class CChatServer : public CNetServer
{
public:
	void Update() noexcept;
	void HeartBeat() noexcept;

	// CNetServer��(��) ���� ��ӵ�
	bool OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept override;
	void OnAccept(const UINT64 sessionID) noexcept override;
	void OnClientLeave(const UINT64 sessionID) noexcept override;
	void OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept override;
	void OnRecv(const UINT64 sessionID, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept override;
	void OnError(int errorcode, WCHAR *errMsg) noexcept override;

	// CNetServer��(��) ���� ��ӵ�
	DWORD OnUpdate() noexcept override;
	void OnHeartBeat() noexcept override;

private:
	// Player ���� ����ü - �̱ۿ��� ���ٵ� ���̹Ƿ� ����ȭ X
};

