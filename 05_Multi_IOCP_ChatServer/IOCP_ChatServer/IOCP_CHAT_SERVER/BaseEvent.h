#pragma once

// Event 수행이 필요하다면 BaseEvent를 상속받고 execute 를 등록
// execute 에 인자를 넣을 경우를 대비하여 가상 함수로 만들지 않음
struct BaseEvent
{
	std::function<void()> execute;		// 이벤트 함수를 정의하고 여기에 함수 포인터를 저장
	DWORD timeMs;
	DWORD nextExecuteTime;
};

struct EventComparator
{
	bool operator()(const BaseEvent *e1, const BaseEvent *e2) const noexcept
	{
		return e1->nextExecuteTime > e2->nextExecuteTime; // 실행시간 빠른 것을 우선
	}
};