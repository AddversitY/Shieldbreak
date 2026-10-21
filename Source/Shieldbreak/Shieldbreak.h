// Shieldbreak game module: shared log category and custom collision channels.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

DECLARE_LOG_CATEGORY_EXTERN(LogShieldbreak, Log, All);

/** Hitscan weapon trace channel. Configured in Config/DefaultEngine.ini as "Weapon", default response Block. */
#define ECC_Weapon ECC_GameTraceChannel1
