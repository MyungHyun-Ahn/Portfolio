#pragma once

namespace Chatting
{
	class CChatContents;

	class CProcessPacket
	{
	public:
		explicit CProcessPacket(CChatContents &contents) noexcept
			: m_contents(contents)
		{
		}
		// message는 CBaseContents가 소유한다.
		bool ConsumePacket(EPacketType type, UINT64 sessionId, NetworkLib::CSerializableBuffer<FALSE> &message) noexcept;

	private:
		bool packetProcReqLogin(UINT64 sessionId, NetworkLib::CSerializableBuffer<FALSE> &message) noexcept;
		bool packetProcReqSectorMove(UINT64 sessionId, NetworkLib::CSerializableBuffer<FALSE> &message) noexcept;
		bool packetProcReqMessage(UINT64 sessionId, NetworkLib::CSerializableBuffer<FALSE> &message) noexcept;
		bool packetProcReqHeartbeat(UINT64 sessionId, NetworkLib::CSerializableBuffer<FALSE> &message) noexcept;
		CChatContents &m_contents;
	};
}
