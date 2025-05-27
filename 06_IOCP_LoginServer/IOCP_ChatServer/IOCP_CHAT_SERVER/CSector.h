#pragma once
class CPlayer;

class CSector
{
public:
	friend class CChatServer;
	friend class CChatProcessPacket;

private:
	std::unordered_map<UINT64, CPlayer *> m_players;
	CDeque<CSerializableBuffer<SERVER_TYPE::WAN> *> m_sendMsgQ;
};