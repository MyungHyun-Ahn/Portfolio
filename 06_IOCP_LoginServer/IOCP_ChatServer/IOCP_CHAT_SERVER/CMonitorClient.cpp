#include "pch.h"
#include "ClientSetting.h"
#include "MonitorSetting.h"
#include "CLanClient.h"
#include "CMonitorClient.h"
#include "CGenPacket.h"

extern CMonitorClient *g_MonitorClient = nullptr;

void CMonitorClient::OnConnect(const UINT64 sessionID) noexcept
{
	CSerializableBuffer<SERVER_TYPE::LAN> *reqLogin = CGenPacket::makePacketReqMonitorLogin(MONITOR_SETTING::SERVER_NO);
	reqLogin->IncreaseRef();

	SendPacket(sessionID, reqLogin);

	if (reqLogin->DecreaseRef() == 0)
	{
		CSerializableBuffer<SERVER_TYPE::LAN>::Free(reqLogin);
	}

	// 로그인 된 sessionID를 Monitor 객체에 등록
	g_monitor.SetMonitorClient(sessionID);
}

void CMonitorClient::OnDisconnect(const UINT64 sessionID) noexcept
{
}

void CMonitorClient::OnRecv(const UINT64 sessionID, CSerializableBufferView<SERVER_TYPE::LAN> *message) noexcept
{
}

void CMonitorClient::RegisterContentTimerEvent() noexcept
{
}
