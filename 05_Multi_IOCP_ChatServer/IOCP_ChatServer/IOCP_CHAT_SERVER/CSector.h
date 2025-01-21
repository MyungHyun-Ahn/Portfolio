#pragma once
class CPlayer;

class CSector
{
public:
	friend class CChatServer;
	friend class CChatProcessPacket;

	CSector() noexcept
	{
		InitializeSRWLock(&m_srwLock);
	}

private:
	SRWLOCK m_srwLock;
	std::unordered_map<UINT64, CPlayer *> m_players;
	// CDeque<CSerializableBuffer<FALSE> *> m_sendMsgQ;
	CLFQueue<CSerializableBuffer<FALSE> *> m_sendMsgLFQ;
};