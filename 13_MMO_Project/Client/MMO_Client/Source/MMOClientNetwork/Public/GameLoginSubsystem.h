#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameLoginSubsystem.generated.h"

namespace GameLoginBridge
{
	class ILogin;
}

enum class EGameLoginPhase : uint8
{
	Idle, Connecting, Waiting, Rejected, Failed, Accepted
};

// 연결은 화면이 아니라 GameInstance에 귀속된다. PIE 인스턴스마다 별도로 정리한다.
UCLASS()
class MMOCLIENTNETWORK_API UGameLoginSubsystem final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	bool SubmitLoginId(const FString& Id);
	EGameLoginPhase GetLoginPhase() const;
	virtual void Deinitialize() override;

private:
	bool TickSession(float DeltaSeconds);
	GameLoginBridge::ILogin* Session = nullptr;
	FTSTicker::FDelegateHandle TickHandle;
	bool bStartFailed = false;
};
