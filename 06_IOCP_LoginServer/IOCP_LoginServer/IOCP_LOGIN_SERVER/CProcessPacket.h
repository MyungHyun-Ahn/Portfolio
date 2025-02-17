#pragma once

class CLoginProcessPacketInterface
{
public:
	virtual bool	ConsumPacket(en_PACKET_TYPE type, UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept = 0;
	virtual bool	PacketProcReqLogin(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept = 0;

	void			SetLoginServer(CLoginServer *loginServer) noexcept { m_pLoginServer = loginServer; }

protected:
	CLoginServer *m_pLoginServer = nullptr;
};

class CLoginProcessPacket : public CLoginProcessPacketInterface
{
public:
	bool ConsumPacket(en_PACKET_TYPE type, UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept override
	{
		switch (type)
		{
		case en_PACKET_CS_LOGIN_REQ_LOGIN:
			return PacketProcReqLogin(sessionId, message);
		}

		return false;
	}

	bool	PacketProcReqLogin(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept override;
};

