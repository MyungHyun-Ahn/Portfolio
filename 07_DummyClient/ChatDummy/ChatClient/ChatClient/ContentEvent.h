#pragma once

struct LoginEvent : public BaseEvent
{
	void SetEvent(UINT64 sessionId) noexcept;
	void Execute(UINT64 sessionId) noexcept;
};
