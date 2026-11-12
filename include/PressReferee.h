#pragma once

// What the mods inside this one are shown of a controller Sprint event while a dodge mod's Sprint-tap dodge is on
// (TK Dodge AIO, Simple Dodge). Free of game types, so tests\test_press_referee.cpp can drive it offline.
//
// WHY (the owner, 2026-11-12, Simple Dodge 1.0.0 with its Sprint tap on): "I'm not able to sprint unless I double tap to
// sprint ... even though it should be set to hold to sprint." Both mods hold the same press:
//   - the dodge mod (inside this one) holds every Sprint event back from the game while the game's heldDownSecs is under
//     its tap window, dodges if the press is let go inside it, and otherwise hands the game the press as a fresh one
//     (heldDownSecs = 0) - but only while the player's sprint flag is off; with the flag on it takes the press for one
//     that STOPS a sprint and passes it on as a plain held event, never as a press;
//   - this mod decides the hold on its own clock and, from then on, writes the sprint flag on after the chain returns.
// Two clocks, one window: when this mod's clock reached the hold time a frame before the game's heldDownSecs reached the
// dodge window, the dodge mod saw the flag already on, never gave the game its press, and the game never started the
// sprint - the flag alone does not run it.
//
// THE RULE. This mod's clock is the one that decides; the dodge mod is told the answer through the held time it reads:
//   - a press not yet decided is shown as held for LESS than the window, so the dodge mod keeps holding it back;
//   - the release of a press not yet decided (a tap) is shown as shorter than the window: a tap, the dodge mod's;
//   - from the event where the press becomes a hold, it is shown as held for at least the window, so the dodge mod lets
//     it through; on the event where it becomes a sprint the sprint flag is cleared first, so the dodge mod hands the game
//     the fresh press that starts the sprint;
//   - the release of a hold is shown as held for at least the window: never a dodge.
// The event's own value is put back after the chain returns, so the game's held time is untouched.

#include <algorithm>

namespace sprint::referee
{
	struct Shown
	{
		float heldSecs = 0.0f;        // the heldDownSecs the inner chain reads
		bool clearFlagFirst = false;  // write the sprint flag off before the chain runs
	};

	// a_window: the dodge mod's tap window in seconds; 0 or less = no Sprint-tap dodge, the event is shown as it came.
	// a_down: a down event (value not 0); otherwise the release. a_decided: this press is a hold by this mod's clock.
	// a_startsSprint: this event is the one where the hold became a sprint. a_reported: the event's own heldDownSecs.
	inline Shown Show(float a_window, bool a_down, bool a_decided, bool a_startsSprint, float a_reported)
	{
		if (a_window <= 0.0f) { return { a_reported, false }; }
		const float below = std::max(a_window - 0.02f, 0.005f);
		if (!a_decided)
		{
			if (a_down) { return { a_reported <= 0.0f ? a_reported : std::min(a_reported, below), false }; }
			// A release must keep a held time above 0, or it is no release (ButtonEvent::IsUp).
			return { std::clamp(a_reported, 0.001f, below), false };
		}
		return { std::max(a_reported, a_window), a_down && a_startsSprint };
	}
}
