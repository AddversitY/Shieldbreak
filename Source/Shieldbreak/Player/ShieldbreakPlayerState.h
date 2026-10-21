// Per-player Slayer stats and the player's tint color. Server-written, replicated to everyone.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ShieldbreakPlayerState.generated.h"

UCLASS()
class SHIELDBREAK_API AShieldbreakPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AShieldbreakPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* NewPlayerState) override;

	// ---- Server only ----

	/** +1 kill; Score mirrors Kills. */
	void AddKill();

	/** Suicide penalty: -1 kill, never below zero. */
	void RemoveKill();

	void AddDeath();
	void ResetStats();

	/** Assigned by AShieldbreakGameMode on join. PaletteIndex lets the GameMode keep colors distinct. */
	void SetPlayerColor(const FLinearColor& NewColor, int32 PaletteIndex = INDEX_NONE);

	// ---- Queries ----

	UFUNCTION(BlueprintPure, Category = "Shieldbreak") int32 GetKills() const { return Kills; }
	UFUNCTION(BlueprintPure, Category = "Shieldbreak") int32 GetDeaths() const { return Deaths; }
	UFUNCTION(BlueprintPure, Category = "Shieldbreak") FLinearColor GetPlayerColor() const { return PlayerColor; }

	/** Palette slot of PlayerColor, or INDEX_NONE before the GameMode assigned one. Server only. */
	int32 GetColorIndex() const { return ColorIndex; }

private:
	UPROPERTY(Replicated)
	int32 Kills = 0;

	UPROPERTY(Replicated)
	int32 Deaths = 0;

	UPROPERTY(Replicated)
	FLinearColor PlayerColor = FLinearColor(0.6f, 0.6f, 0.6f, 1.f);

	int32 ColorIndex = INDEX_NONE;
};
