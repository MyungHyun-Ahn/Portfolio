#include "Modules/ModuleManager.h"
#include "Misc/AutomationTest.h"

THIRD_PARTY_INCLUDES_START
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winsock2.h>
#include <mswsock.h>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include "PacketHeaders.h"
#include "PacketFraming.h"
#include "CClientManager.h"
#include "SClientNetworkSettings.h"
#include "Windows/HideWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_END

static_assert(_MSC_FULL_VER == 195035740 && _ITERATOR_DEBUG_LEVEL == 0);

IMPLEMENT_MODULE(FDefaultModuleImpl, MMOClientNetwork)

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClientNetworkInitStopTest, "MMO.ClientNetwork.InitStopNoConnect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FClientNetworkInitStopTest::RunTest(const FString& Parameters)
{
	ClientNetworkLib::CClientManager network;
	const ClientNetworkLib::SClientNetworkSettings settings{};
	if (!TestTrue(TEXT("Native Init"), network.Init(settings)))
	{
		return false;
	}
	TestFalse(TEXT("Duplicate Init while running"), network.Init(settings));
	network.Stop();
	// 같은 객체에서 재초기화해야 Stop이 실제로 IOCP와 워커를 정리했는지 확인할 수 있다.
	if (!TestTrue(TEXT("Native Init after Stop"), network.Init(settings)))
	{
		return false;
	}
	network.Stop();
	AddInfo(TEXT("event=client_network_init_stop cycles=2 | 무접속 초기화 및 해제 완료"));
	return true;
}
#endif
