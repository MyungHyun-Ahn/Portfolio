#pragma once
class CProcessPacketInterface
{
public:
	virtual bool	ConsumePacket(PACKET_CODE code, UINT64 sessionId, CSerializableBuffer *message) = 0;
	{%- for pkt in csList %}
	virtual bool 	PacketProc{{pkt.name}}(UINT64 sessionId, CSerializableBuffer *message) = 0;
	{%- endfor %}
	inline void		SetServer(CServerCore *server) { m_pServerCore = server; }

protected:
	CServerCore *m_pServerCore;
};

class CGameProcessPacket : public CProcessPacketInterface
{
public:
	bool ConsumePacket(PACKET_CODE code, UINT64 sessionId, CSerializableBuffer *message)
	{
		switch (code)
		{
		{%- for pkt in csList %}
		case PACKET_CODE::{{pkt.name}}:
			return PacketProc{{pkt.name}}(sessionId, message);
		{%- endfor %}
		default:
			break;
		}

		return false;
	}

	{%- for pkt in csList %}
	bool PacketProc{{pkt.name}}(UINT64 sessionId, CSerializableBuffer *message);
	{%- endfor %}
};