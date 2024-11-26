#pragma once
class CClient
{
public:
	CClient()
	{
		InitializeSRWLock(&m_srwLock);
	}

	INT m_iPrevSendTime;
	INT m_iCreateTime;
	bool m_bLoginPacketRecv = false;

	UINT64 m_iSendNum = 0;
	UINT64 m_iRecvNum = 0;
	SRWLOCK m_srwLock;
};

class CEchoClients : public CLanClients
{
public:
	CEchoClients()
	{
		InitializeSRWLock(&m_srwLock);
	}

	void OnConnect(const UINT64 sessionID);
	void OnRecv(const UINT64 sessionID, CSmartPtr<CSerializableBufferView> message);
	void OnReleaseSession(const UINT64 sessionID);

public:
	std::unordered_map<UINT64, CClient *> m_clientMap;
	SRWLOCK m_srwLock;
};

extern int oversendCount;