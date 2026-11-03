// Per-map ambient sound: a looping bed (wind, a ship's hum) plus a few placed sources (Creekbed's creek) and occasional
// birds on the green maps. Synthesized like the effects in ShieldbreakSFX, except Bastion's and Sanctum's wind, which
// use the wind loop already in the project's content when it is there. Visual maps only; nothing here touches gameplay.

#pragma once

#include "CoreMinimal.h"

class AActor;

namespace ShieldbreakAmbience
{
	/** Starts the ambience for the arena's map. Clients and listen servers only; skips the front end and -nosound runs.
	 *  -SBNoAmbience turns it off for comparisons. Everything stops with the arena. */
	SHIELDBREAK_API void Start(AActor* Arena, FName MapId, bool bIndoor, const TArray<FBox>& WaterSheets);
}
