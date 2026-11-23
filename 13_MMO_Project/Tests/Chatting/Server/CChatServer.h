#pragma once

namespace Chatting
{
	class CChatContents;

	class CChatServer : public ServerNetworkLib::CServer
	{
	public:
		CChatServer() noexcept;
		bool OnConnectionRequest(const WCHAR *pIp, USHORT port) noexcept override;
		void OnAccept(UINT64 sessionId) noexcept override;
		void OnClientLeave(UINT64 sessionId) noexcept override;
		void OnRecv(UINT64 sessionId, NetworkCommonLib::CSerializableBufferView *pMessage) noexcept override;
		void OnError(int errorCode, WCHAR *pMessage) noexcept override;
		void RegisterContentTimerEvent() noexcept override;

	private:
		// 서버와 콘텐츠는 프로세스 종료까지 유지한다.
		CChatContents *m_pContents;
	};
}
