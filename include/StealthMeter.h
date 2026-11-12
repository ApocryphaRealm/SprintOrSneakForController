#pragma once

// The HUD's sneak eye after this mod changes crouch (1.1.1; Nexus report, 2026-11-12: "a persistent sneak eye icon even
// after I stop sneaking"). Free of game types, so tests\test_stealth_meter.cpp can drive it offline.
//
// HOW THE GAME SHOWS THE EYE (read from the 1.5.97 code; the 1.7.104 StealthMeter reads the same message):
//   - the eye is the HUD's StealthMeter. It is shown and hidden ONLY by a kCrosshairSneak (24) HUD message - show at
//     HUDData +0x40, the detection level at +0x44 - that StealthMeter::ProcessMessage (1.5.97 id 50787, 1.7.104 id 51683)
//     turns into FadeIn or FadeOut and the StealthMode HUD mode. Nothing reads the player's sneak state from the HUD side.
//   - one function sends it, the actor's stealth update (1.5.97 id 39514): show = the ActorState sneaking bit is set (and
//     0x400 clear). The player's per-frame update calls it ONLY while that bit is set (gate id 19632), so the eye comes up
//     on the first frame of a sneak however it began, but the frame after the bit clears, nothing calls it any more.
//   - the game's own sneak changes therefore call the stealth update themselves after the change: the Sneak action
//     (kActionSneak 59, through the action queue id 41271 -> id 39495) and the game's other clears (id 36901).
// This mod changes crouch with the setter's own writes (source/Sprint.cpp SetSneaking), not through the action, so the
// game's refresh after a stand-up never ran: the last message was "show", and the eye stayed up over a standing player.
// The player really stood up - the state was right; only the HUD was not told.
//
// THE RULE. After this mod stands the player up, the HUD is sent what the game's stealth update would have sent with the
// sneaking bit clear: kCrosshairSneak, show off, level 0 (what the HUD's own reset sends, id 50718). Crouching down needs
// nothing: the per-frame update shows the eye on the next frame, as it always did.

namespace sprint::stealth
{
	struct Message
	{
		bool send = false;  // send a kCrosshairSneak message to the HUD now
		bool show = false;  // its show flag
		int level = 0;      // its detection level (0..100); 0 with show off
	};

	// a_before: the sneaking bit before this mod's change; a_after: after it.
	inline Message AfterCrouchChange(bool a_before, bool a_after)
	{
		Message m;
		if (a_before && !a_after) { m.send = true; }
		return m;
	}
}
