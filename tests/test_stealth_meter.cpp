// Offline test of include/StealthMeter.h - Sprint or Sneak 1.1.1's fix for the Nexus report of 2026-11-12: "a persistent
// sneak eye icon even after I stop sneaking. Installing Contextual Crosshair fixed it."
//
// Build and run (any C++17 compiler):  clang++ -std=c++17 -I ../include test_stealth_meter.cpp -o t.exe && t.exe
// tests\run-tests.ps1 does that. Exit code 0 = every case passed.
//
// Models, each a copy of the code it stands for (1.5.97, read from the game's code; see include/StealthMeter.h):
//   Hud      - StealthMeter: the eye is up after a kCrosshairSneak message with show on, down after one with show off; it
//              reads nothing else.
//   Stealth  - the actor's stealth update (id 39514): sends kCrosshairSneak with show = the sneaking bit.
//   Frame    - the player's per-frame update (id 39375): calls Stealth only while the sneaking bit is set (gate id 19632).
//   Action   - the game's own Sneak (kActionSneak through id 41271 -> id 39495): toggles the bit, then calls Stealth.
//   Mod      - source/Sprint.cpp SetSneaking: the setter's writes (the bit), then, in 1.1.1, the message StealthMeter.h says.

#include "StealthMeter.h"

#include <cstdio>
#include <string>

static int g_fail = 0;
static int g_n = 0;
static void Check(bool a_ok, const std::string& a_what)
{
	++g_n;
	std::printf("%s %d %s\n", a_ok ? "PASS" : "FAIL", g_n, a_what.c_str());
	if (!a_ok) { ++g_fail; }
}

struct World
{
	bool sneaking = false;  // ActorState sneaking bit
	bool eye = false;       // the HUD's sneak eye
	int messages = 0;       // kCrosshairSneak messages the HUD got
	bool fixed = true;      // Mod is 1.1.1 (true) or 1.1.0 (false)
};

static void Hud(World& w, bool a_show)
{
	++w.messages;
	w.eye = a_show;
}

static void Stealth(World& w) { Hud(w, w.sneaking); }

static void Frame(World& w)
{
	if (w.sneaking) { Stealth(w); }
}

static void Action(World& w)
{
	w.sneaking = !w.sneaking;
	Stealth(w);
}

static void Mod(World& w, bool a_on)
{
	const bool before = w.sneaking;
	if (before == a_on) { return; }
	w.sneaking = a_on;
	if (!w.fixed) { return; }
	const auto msg = sprint::stealth::AfterCrouchChange(before, w.sneaking);
	if (msg.send) { Hud(w, msg.show); }
}

static void Frames(World& w, int a_n)
{
	for (int i = 0; i < a_n; ++i) { Frame(w); }
}

int main()
{
	// The rule itself.
	{
		const auto up = sprint::stealth::AfterCrouchChange(true, false);
		Check(up.send && !up.show && up.level == 0, "stand-up: kCrosshairSneak sent, show off, level 0 (what the game sends with the bit clear)");
		const auto down = sprint::stealth::AfterCrouchChange(false, true);
		Check(!down.send, "crouch: nothing sent (the per-frame update shows the eye)");
		Check(!sprint::stealth::AfterCrouchChange(false, false).send && !sprint::stealth::AfterCrouchChange(true, true).send, "no change: nothing sent");
	}

	// The game alone: the eye follows its own Sneak both ways.
	{
		World w;
		Action(w); Frames(w, 5);
		Check(w.sneaking && w.eye, "vanilla Sneak: eye up while crouched");
		Action(w); Frames(w, 5);
		Check(!w.sneaking && !w.eye, "vanilla Sneak: eye down after standing up");
	}

	for (int fixed = 0; fixed < 2; ++fixed)
	{
		const std::string tag = fixed ? "1.1.1" : "1.1.0";
		// A still hold crouches, the next still hold stands up.
		{
			World w;
			w.fixed = fixed;
			Mod(w, true); Frames(w, 5);
			Check(w.sneaking && w.eye, tag + " still hold crouches: eye up from the next frame");
			Mod(w, false); Frames(w, 30);
			const bool stuck = !w.sneaking && w.eye;
			Check(fixed ? (!w.sneaking && !w.eye) : stuck, tag + (fixed ? " still hold stands up: eye down" : " still hold stands up: eye STAYS up over a standing player (the report reproduced)"));
		}
		// Crouched with the game's Sneak, a moving hold stands up and sprints.
		{
			World w;
			w.fixed = fixed;
			Action(w); Frames(w, 5);
			Mod(w, false); Frames(w, 30);
			Check(fixed ? !w.eye : w.eye, tag + (fixed ? " vanilla crouch, moving hold stands up and sprints: eye down" : " vanilla crouch, moving hold stands up: eye stays (the report reproduced)"));
		}
		// Crouched by the mod, stood up with the game's Sneak: the game's own refresh hides it in both versions.
		{
			World w;
			w.fixed = fixed;
			Mod(w, true); Frames(w, 5);
			Action(w); Frames(w, 5);
			Check(!w.sneaking && !w.eye, tag + " mod crouch, vanilla Sneak stands up: eye down");
		}
		// Many rounds, mixed: the eye always matches the state once a frame has passed.
		{
			World w;
			w.fixed = fixed;
			bool allMatch = true;
			for (int i = 0; i < 40; ++i)
			{
				if (i % 3 == 0) { Action(w); }
				else { Mod(w, !w.sneaking); }
				Frames(w, 2);
				if (w.eye != w.sneaking) { allMatch = false; }
			}
			Check(fixed ? allMatch : !allMatch, tag + (fixed ? " 40 mixed crouch changes: the eye always matches the state" : " 40 mixed crouch changes: the eye is left wrong at least once"));
		}
	}

	// Only one message per stand-up, none per frame while standing (the HUD is not flooded).
	{
		World w;
		Mod(w, true); Frames(w, 3);
		const int before = w.messages;
		Mod(w, false); Frames(w, 100);
		Check(w.messages == before + 1, "1.1.1 stand-up: one message, then none while standing");
	}

	std::printf("%d of %d passed\n", g_n - g_fail, g_n);
	return g_fail ? 1 : 0;
}
