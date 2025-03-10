#pragma once
class CAuthContent : public CBaseContent
{
	// CBaseContent을(를) 통해 상속됨
	void OnEnter(const UINT64 sessionID, void *pObject) noexcept override;
	void OnLeave(const UINT64 sessionID) noexcept override;
	RECV_RET OnRecv(const UINT64 sessionID, CSerializableBuffer<FALSE> *message) noexcept override;
	void OnLoopEnd() noexcept override;
};

class CEchoContent : public CBaseContent
{
	// CBaseContent을(를) 통해 상속됨
	void OnEnter(const UINT64 sessionID, void *pObject) noexcept override;
	void OnLeave(const UINT64 sessionID) noexcept override;
	RECV_RET OnRecv(const UINT64 sessionID, CSerializableBuffer<FALSE> *message) noexcept override;
	void OnLoopEnd() noexcept override;
};