// Objective-mode actors. The game mode spawns them from the map's objective locations:
// - AShieldbreakCarriable: a team flag (Capture the Flag) or the ball (Oddball). Picked up by walking over it,
//   carried on the back, dropped on death, returned home after a timeout or (flags) when touched by their own team.
// - AShieldbreakHill: the King of the Hill zone. The game mode scores it and moves it around the map.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShieldbreakObjectives.generated.h"

class AShieldbreakCharacter;
class UBoxComponent;
class USphereComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EShieldbreakGameType : uint8
{
	Slayer,
	CaptureTheFlag,
	KingOfTheHill,
	Oddball
};

UENUM(BlueprintType)
enum class EShieldbreakCarriableKind : uint8
{
	Flag,
	Ball
};

UCLASS()
class SHIELDBREAK_API AShieldbreakCarriable : public AActor
{
	GENERATED_BODY()

public:
	AShieldbreakCarriable();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server, right after spawning. Team is ignored for the ball. */
	void Setup(EShieldbreakCarriableKind InKind, uint8 InTeam, const FVector& InHomeLocation);

	EShieldbreakCarriableKind GetKind() const { return Kind; }
	uint8 GetTeam() const { return Team; }
	AShieldbreakCharacter* GetCarrier() const { return Carrier; }
	bool IsAtHome() const { return bAtHome; }
	bool IsDropped() const { return !bAtHome && !Carrier; }
	FVector GetHomeLocation() const { return HomeLocation; }

	/** Server: drop where the carrier stands (carrier died). */
	void Drop();
	/** Server: snap back to the home location. */
	void ReturnHome();

	/** Seconds a dropped flag / ball waits before returning home on its own. */
	UPROPERTY(EditAnywhere, Category = "Objective")
	float FlagReturnTime = 15.f;

	UPROPERTY(EditAnywhere, Category = "Objective")
	float BallReturnTime = 30.f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Objective") TObjectPtr<USphereComponent> Trigger;
	UPROPERTY(VisibleAnywhere, Category = "Objective") TObjectPtr<UStaticMeshComponent> PoleMesh;
	UPROPERTY(VisibleAnywhere, Category = "Objective") TObjectPtr<UStaticMeshComponent> BannerMesh;
	UPROPERTY(VisibleAnywhere, Category = "Objective") TObjectPtr<UStaticMeshComponent> BaseMesh;

private:
	/** Server: pick-up / return / capture logic for a character touching this objective. */
	void HandleTouch(AShieldbreakCharacter* Character);
	void PickUp(AShieldbreakCharacter* Character);
	void UpdateVisuals();

	UFUNCTION() void OnRep_Carrier();
	UFUNCTION() void OnRep_Setup();

	UPROPERTY(ReplicatedUsing = OnRep_Setup) EShieldbreakCarriableKind Kind = EShieldbreakCarriableKind::Flag;
	UPROPERTY(ReplicatedUsing = OnRep_Setup) uint8 Team = 255;
	UPROPERTY(ReplicatedUsing = OnRep_Carrier) TObjectPtr<AShieldbreakCharacter> Carrier;
	UPROPERTY(Replicated) bool bAtHome = true;
	UPROPERTY(Replicated) FVector HomeLocation = FVector::ZeroVector;

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BannerMID;

	float DroppedServerTime = -1.f;
	TWeakObjectPtr<AShieldbreakCharacter> LastDropper;
	float LastDropTime = -100.f;
};

UCLASS()
class SHIELDBREAK_API AShieldbreakHill : public AActor
{
	GENERATED_BODY()

public:
	AShieldbreakHill();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: which teams have living players inside. Returns the single holding team, NoTeam if empty, or
	 *  ContestedTeam when both teams are in. */
	uint8 EvaluateControl();

	/** Server: move the zone. */
	void MoveTo(const FVector& NewLocation);

	uint8 GetControllingTeam() const { return ControllingTeam; }

	static constexpr uint8 ContestedTeam = 254;

	UPROPERTY(EditAnywhere, Category = "Objective")
	FVector Extent = FVector(300.f, 300.f, 220.f);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Objective") TObjectPtr<UBoxComponent> Zone;

private:
	void BuildBorder();
	void UpdateVisuals();

	UFUNCTION() void OnRep_ControllingTeam();

	UPROPERTY(ReplicatedUsing = OnRep_ControllingTeam) uint8 ControllingTeam = 255;

	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> BorderPieces;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> BorderMIDs;
};
