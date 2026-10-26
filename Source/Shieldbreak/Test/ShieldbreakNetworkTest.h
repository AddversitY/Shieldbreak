#pragma once

class AShieldbreakPlayerController;

/** Opt-in two-process regression probes; no timers or probes run in normal games. */
struct FShieldbreakNetworkTest
{
	static void Start(AShieldbreakPlayerController* TestController);
};
