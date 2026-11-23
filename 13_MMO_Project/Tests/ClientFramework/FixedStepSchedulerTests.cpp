#include <chrono>
#include "CFixedStepScheduler.h"

int main(void)
{
	using ClientFrameworkLib::CFixedStepScheduler;
	using namespace std::chrono_literals;

	CFixedStepScheduler accumulated;
	if (accumulated.Advance(8ms) != 0 || accumulated.Advance(9ms) != 1)
		return 1;

	CFixedStepScheduler capped;
	if (capped.Advance(210ms) != 4 || capped.Advance(0ns) != 4 || capped.Advance(0ns) != 4
		|| capped.Advance(0ns) != 0 || capped.Advance(7ms) != 1)
		return 2;

	CFixedStepScheduler exact;
	int updates = 0;
	for (int frame = 0; frame < 100; ++frame)
		updates += exact.Advance(10ms);
	if (updates != 60)
		return 3;

	CFixedStepScheduler paused;
	if (paused.Advance(1010ms) != 4)
		return 4;
	for (int batch = 1; batch < 15; ++batch)
	{
		if (paused.Advance(0ns) != 4)
			return 4;
	}
	if (paused.Advance(0ns) != 0 || paused.Advance(7ms) != 1)
		return 4;

	CFixedStepScheduler invalid;
	if (invalid.Advance(10ms) != 0 || invalid.Advance(-10ms) != 0 || invalid.Advance(7ms) != 1)
		return 5;
	return 0;
}