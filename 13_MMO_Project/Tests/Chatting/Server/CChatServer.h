#pragma once

namespace Chatting
{
	class CChatContents;

	class CChatServer : public ServerNetworkLib::CServer
	{
	public:
		CChatServer(std::size_t contentsWorkerCount, std::chrono::milliseconds frameInterval);
		[[nodiscard]]
		bool StartContents();
		void StopContents();
		bool OnConnectionRequest(const WCHAR *pIp, USHORT port) noexcept override;
		void OnAccept(UINT64 sessionId) noexcept override;
		void OnClientLeave(UINT64 sessionId) noexcept override;
		void OnError(int errorCode, WCHAR *pMessage) noexcept override;
		void OnServerReady(USHORT port) noexcept override;

	private:
		static constexpr ContentsRuntimeLib::ContentsInstanceId kChatContentsId = 1;
		ContentsRuntimeLib::CContentsRuntime m_runtime;
		ContentsRuntimeLib::ContentsHandle m_completionHandle;
		CChatContents *m_pContents = nullptr;
	};
}
