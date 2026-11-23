#pragma once

namespace Chatting
{
	class CChatContents;

	class CProcessPacket final : public CServerPacketSender, public IServerPacketHandler
	{
	public:
		explicit CProcessPacket(CChatContents &contents) noexcept
			: m_contents(contents)
		{
		}
		bool HandleReqLogin(UINT64 sessionId, const SReqLogin &packet) noexcept override;
		bool HandleReqSectorMove(UINT64 sessionId, const SReqSectorMove &packet) noexcept override;
		bool HandleReqMessage(UINT64 sessionId, const SReqMessage &packet) noexcept override;
		bool HandleHeartbeatNoti(UINT64 sessionId, const SHeartbeatNoti &packet) noexcept override;

	private:
		void sendPacket(UINT64 sessionId, NetworkCommonLib::CSerializableBuffer &packet) noexcept override;
		CChatContents &m_contents;
	};
}
