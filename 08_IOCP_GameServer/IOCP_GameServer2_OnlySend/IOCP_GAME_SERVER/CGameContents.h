#pragma once
class CAuthContents : public CBaseContents
{
	// CBaseContent��(��) ���� ��ӵ�
	void OnEnter(const UINT64 sessionID, void *pObject) noexcept override;
	void OnLeave(const UINT64 sessionID) noexcept override;
	RECV_RET OnRecv(const UINT64 sessionID, CSerializableBuffer<FALSE> *message, int delayFrame) noexcept override;
	void OnLoopEnd() noexcept override;
};

class CEchoContents : public CBaseContents
{
	// CBaseContent��(��) ���� ��ӵ�
	void OnEnter(const UINT64 sessionID, void *pObject) noexcept override;
	void OnLeave(const UINT64 sessionID) noexcept override;
	RECV_RET OnRecv(const UINT64 sessionID, CSerializableBuffer<FALSE> *message, int delayFrame) noexcept override;
	void OnLoopEnd() noexcept override;
};