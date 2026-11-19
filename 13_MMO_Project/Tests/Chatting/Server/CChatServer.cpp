#include "pch.h"
#include "NetworkLimits.h"
#include "COverlappedAllocator.h"
#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CRecvBuffer.h"
#include "CSerializableBufferView.h"
#include "SBaseEvent.h"
#include "ServerSetting.h"
#include "CMonitor.h"
#include "CNetServer.h"
#include "CBaseContents.h"
#include "SystemEvent.h"
#include "CContentsThread.h"
#include "CommonProtocol.h"
#include "CSector.h"
#include "IServerPacketHandler.g.h"
#include "CProcessPacket.h"
#include "CChatContents.h"
#include "CChatServer.h"

namespace Chatting
{
	using MHLib::utils::ELogLevel;
	using MHLib::utils::g_pLogger;

	struct SChatFrameEvent : public NetworkLib::SContentsFrameEvent
	{
		void Execute(int delayFrame) noexcept override
		{
			NetworkLib::SContentsFrameEvent::Execute(delayFrame);
			m_pBaseContent->OnLoopEnd();
		}
	};

	CChatServer::CChatServer() noexcept
		: m_pContents(new CChatContents(*this))
	{
	}

	bool CChatServer::OnConnectionRequest(const WCHAR *pIp, USHORT /*port*/) noexcept
	{
		// 고정 테스트 키는 외부 접속에 개방하지 않는다.
		return wcscmp(pIp, L"127.0.0.1") == 0;
	}

	void CChatServer::OnAccept(UINT64 sessionId) noexcept
	{
		m_pContents->MoveJobEnqueue(sessionId, nullptr);
	}

	void CChatServer::OnClientLeave(UINT64 /*sessionId*/) noexcept
	{
		// NetworkLib의 LeaveJobEnqueue 이후 콘텐츠 프레임에서 정리한다.
	}

	void CChatServer::OnRecv(UINT64 /*sessionId*/, NetworkLib::CSerializableBufferView<FALSE> * /*pMessage*/) noexcept
	{
		// 현재 NetworkLib는 CBaseContents의 수신 큐를 사용한다.
	}

	void CChatServer::OnError(int errorCode, WCHAR *pMessage) noexcept
	{
		g_pLogger->WriteLogConsole(ELogLevel::Error, L"NETWORK error=%d %ls", errorCode, pMessage != nullptr ? pMessage : L"");
	}

	void CChatServer::RegisterContentTimerEvent() noexcept
	{
		auto *pEvent = new SChatFrameEvent;
		pEvent->SetEvent(m_pContents, 25);
		NetworkLib::CContentsThread::EnqueueEvent(pEvent);
		g_pLogger->WriteLogConsole(ELogLevel::System, L"READY chat test-only port=%u", NetworkLib::serverSetting::openPort);
	}
}
