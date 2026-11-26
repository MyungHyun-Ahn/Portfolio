#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "IChatLoginSession.h"
#include "ChatLoginWorldSubsystem.generated.h"

UCLASS()
class UChatLoginWorldSubsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	bool ShouldCreateSubsystem(UObject* Outer) const override;
	void OnWorldBeginPlay(UWorld& World) override;
	void Tick(float DeltaTime) override;
	TStatId GetStatId() const override;
	void Deinitialize() override;

private:
	TUniquePtr<Chatting::IChatLoginSession> Session;
	int64 Account = 0;
};
