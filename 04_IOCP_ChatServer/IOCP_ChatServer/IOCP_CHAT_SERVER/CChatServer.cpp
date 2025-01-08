#include "pch.h"
#include "CNetServer.h"
#include "CNetSession.h"
#include "CChatServer.h"

#define FPS 25
#define FRAME_PER_TICK 1000 / FPS

bool CChatServer::OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept
{
	return false;
}

void CChatServer::OnAccept(const UINT64 sessionID) noexcept
{
}

void CChatServer::OnClientLeave(const UINT64 sessionID) noexcept
{
}

void CChatServer::OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept
{
}

void CChatServer::OnRecv(const UINT64 sessionID, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept
{
}

void CChatServer::OnError(int errorcode, WCHAR *errMsg) noexcept
{
}

DWORD CChatServer::OnUpdate() noexcept
{
	static DWORD prevTick = timeGetTime();

	int dTime = timeGetTime() - prevTick;
	// 다음 깨어나야 하는 시간
	if (dTime - prevTick < FRAME_PER_TICK)
		return FRAME_PER_TICK - dTime;

	// 진짜 Update
	Update();

	prevTick += FRAME_PER_TICK;
	return FRAME_PER_TICK - (timeGetTime() - prevTick);
}

void CChatServer::OnHeartBeat() noexcept
{
	// non-login Player 객체의 HeartBeat 검사
	static DWORD nonLoginPrevTick = timeGetTime();
	int dTime = timeGetTime() - nonLoginPrevTick;
	if (dTime - nonLoginPrevTick < 1000 * 3) // 비교적 짧은 시간
		return;

	// HeartBeat 동작


	nonLoginPrevTick += 1000 * 3;

	// login Player 객체의 HeartBeat 검사
	static DWORD loginPrevTick = timeGetTime();
	int dTime = timeGetTime() - loginPrevTick;
	if (dTime - loginPrevTick < 1000 * 40)
		return;

	// HeartBeat 동작


	loginPrevTick += 1000 * 40;
}
