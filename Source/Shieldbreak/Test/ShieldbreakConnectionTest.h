#pragma once

class AShieldbreakPlayerController;

/** Opt-in connection recovery probes. No timers or attempts run in ordinary games. */
struct FShieldbreakConnectionTest
{
	static void Start(AShieldbreakPlayerController* TestController);
};
