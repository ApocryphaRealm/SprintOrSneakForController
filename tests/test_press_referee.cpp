// Offline test of include/PressReferee.h - Sprint or Sneak 1.1.0's fix for the owner's report with Simple Dodge 1.0.0's
// Sprint-tap dodge on (2026-11-12): "I'm not able to sprint unless I double tap to sprint".
//
// Build and run (any C++17 compiler):  clang++ -std=c++17 -I ../include test_press_referee.cpp -o t.exe && t.exe
// tests\run-tests.ps1 does that. Exit code 0 = every case passed.
//
// The chain on a controller Sprint event, outermost first: Sprint or Sneak -> the dodge mod's SprintHandler hook -> the
// game's SprintHandler. The event streams are what the game sends: one down event per frame while the button is held
// (heldDownSecs 0 on the first, then rising) and one release (value 0, heldDownSecs the time held).
//
// Models, each a copy of the code it stands for:
//   Sos     - source/Sprint.cpp's ProcessButton, the press classification only (own clock; moving -> sprint, still -> crouch
//             once; the flag written after the chain). old = 1.0.0 (event passed as it came), new = 1.1.0 (PressReferee).
//   Dodge   - Simple Dodge's SprintHandlerHook: v100 = git 7cc06ca src/Hooks.cpp (same code as TK Dodge AIO), v110 =
//             src/Hooks.cpp HandleTapKey + src/TapDetector.h. Menus are left out.
//   Game    - the vanilla handler flips the sprint flag on a press (IsDown); the player sprints only when the flag is on AND
//             the handler got a press this hold. That second condition is the hypothesis (falsification episode 125): the
//             owner's log has 14 "hold decided: moving" lines, so 1.0.0 wrote the flag on through every one, and he did not sprint.
//   gameLag - the game's heldDownSecs running behind this mod's clock by that much (the race the owner lost every time).

#include "PressReferee.h"

#include <algorithm>
#include <cmath>
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

struct Event
{
	float value;
	float held;
	bool IsDown() const { return value != 0.0f && held == 0.0f; }
	bool IsUp() const { return value == 0.0f && held != 0.0f; }
};

struct World
{
	bool flag = false;          // PlayerCharacter isSprinting
	bool gotPress = false;      // the game's handler got a press during this hold
	bool sneaking = false;
	bool moving = false;
	int dodges = 0;
	int sprintFrames = 0;       // frames the player really sprinted
	int crouchChanges = 0;
	int gamePresses = 0;
};

// ---- the game ----
static void Game(World& w, Event& e)
{
	if (e.IsDown())
	{
		w.flag = !w.flag;
		w.gotPress = true;
		++w.gamePresses;
	}
}

// ---- the dodge mod ----
struct Dodge
{
	int version = 110;    // 100 or 110
	float window = 0.30f; // fSprintingPressDuration
	bool doubleTap = false;
	// 1.0.0 state
	bool sawDown = false, stopping = false;
	double lastInput = -10.0;
	// 1.1.0 state
	bool held = false;
	double downAt = 0.0;
	bool armed = false;
	double firstUp = 0.0;

	void Input100(World& w, double now)
	{
		if (doubleTap)
		{
			if (now - lastInput <= 0.30) { ++w.dodges; lastInput = -10.0; }
			else { lastInput = now; }
		}
		else { ++w.dodges; }
	}

	// Returns true when the event was eaten (not passed to the game).
	bool Process(World& w, Event& e, double now)
	{
		const bool sprinting = w.flag;
		if (version == 100)
		{
			if (e.IsDown()) { sawDown = true; }
			if (e.IsDown() && sprinting) { stopping = true; }
			else if (e.held < window)
			{
				if (e.IsUp())
				{
					if (sawDown) { Input100(w, now); }
					sawDown = false;
					stopping = false;
				}
				return true;
			}
			else if (!sprinting && !stopping) { e.held = 0.0f; }
			else if (e.IsUp()) { stopping = false; }
			return false;
		}
		// 1.1.0
		const bool up = e.IsUp();
		if (e.IsDown() && !held) { held = true; downAt = now; }
		if (e.IsDown() && sprinting) { stopping = true; }
		else if (e.held < window)
		{
			if (!up) { return true; }
			const float own = held ? static_cast<float>(now - downAt) : -1.0f;
			held = false;
			const bool tap = own >= 0.0f && own < window && e.held < window;
			if (tap)
			{
				if (doubleTap)
				{
					if (armed && downAt - firstUp <= 0.30) { armed = false; ++w.dodges; }
					else { armed = true; firstUp = now; }
				}
				else { ++w.dodges; }
			}
			else if (own >= 0.0f) { armed = false; }
			stopping = false;
			if (tap) { return true; }
		}
		else if (!sprinting && !stopping) { e.held = 0.0f; }
		else if (up) { stopping = false; }
		if (up && held) { held = false; armed = false; }
		return false;
	}
};

// ---- Sprint or Sneak ----
enum class Mode { kNone, kUndecided, kSprint, kCrouch };

struct Sos
{
	bool fixed = true;     // 1.1.0 referee
	float holdSecs = 0.30f;
	float refereeSecs = 0.30f;  // 0 = no dodge mod's sprint tap
	Mode mode = Mode::kNone;
	double pressedAt = 0.0;
	bool crouchChanged = false;

	void Chain(World& w, Dodge* d, Event& e, double now, bool down, bool decided, bool startsSprint)
	{
		const float reported = e.held;
		if (fixed)
		{
			const auto shown = sprint::referee::Show(refereeSecs, down, decided, startsSprint, reported);
			if (shown.clearFlagFirst) { w.flag = false; }
			e.held = shown.heldSecs;
		}
		if (!d || !d->Process(w, e, now)) { Game(w, e); }
		e.held = reported;
	}

	void Process(World& w, Dodge* d, Event e, double now)
	{
		if (e.value != 0.0f)
		{
			if (mode == Mode::kNone) { mode = Mode::kUndecided; pressedAt = now; crouchChanged = false; }
			const Mode before = mode;
			if (now - pressedAt >= holdSecs - 1e-6)
			{
				if (w.moving)
				{
					if (!w.sneaking) { mode = Mode::kSprint; }
					else if (!crouchChanged) { w.sneaking = false; crouchChanged = true; ++w.crouchChanges; mode = Mode::kSprint; }
				}
				else if (mode == Mode::kUndecided && !crouchChanged)
				{
					w.sneaking = !w.sneaking;
					crouchChanged = true;
					++w.crouchChanges;
					mode = Mode::kCrouch;
				}
			}
			if (mode != Mode::kCrouch) { Chain(w, d, e, now, true, mode == Mode::kSprint, before != Mode::kSprint && mode == Mode::kSprint); }
			w.flag = mode == Mode::kSprint;
		}
		else
		{
			if (mode == Mode::kNone) { if (!d || !d->Process(w, e, now)) { Game(w, e); } }
			else { Chain(w, d, e, now, false, mode != Mode::kUndecided, false); }
			w.flag = false;
			mode = Mode::kNone;
		}
		// The player's update this frame.
		if (w.flag && w.gotPress && w.moving) { ++w.sprintFrames; }
		if (e.value == 0.0f) { w.gotPress = false; }
	}
};

struct Press { double from, to; };

struct Setup
{
	int dodgeVersion = 110;  // 0 = no dodge mod
	bool sprintTap = true;
	bool doubleTap = false;
	float window = 0.30f;
	bool fixed = true;
	float holdSecs = 0.30f;
	double gameLag = 0.0;    // seconds the game's heldDownSecs runs behind
	bool moving = true;
	bool sneaking = false;
};

static World Run(const Setup& s, std::initializer_list<Press> a_presses)
{
	World w;
	w.moving = s.moving;
	w.sneaking = s.sneaking;
	Dodge d;
	d.version = s.dodgeVersion;
	d.window = s.window;
	d.doubleTap = s.doubleTap;
	Dodge* dp = (s.dodgeVersion && s.sprintTap) ? &d : nullptr;  // bUseSprintKey off: the hook passes everything
	Sos sos;
	sos.fixed = s.fixed;
	sos.holdSecs = s.holdSecs;
	sos.refereeSecs = dp ? s.window : 0.0f;
	const double dt = 1.0 / 60.0;
	for (const auto& p : a_presses)
	{
		double t = p.from;
		for (; t < p.to - 1e-9; t += dt)
		{
			const float own = static_cast<float>(t - p.from);
			const float game = own == 0.0f ? 0.0f : std::max(0.0001f, static_cast<float>(own - s.gameLag));
			sos.Process(w, dp, Event{ 1.0f, game }, t);
		}
		const float own = static_cast<float>(t - p.from);
		sos.Process(w, dp, Event{ 0.0f, std::max(0.001f, static_cast<float>(own - s.gameLag)) }, t);
	}
	return w;
}

static std::string Name(const Setup& s)
{
	char buf[160];
	std::snprintf(buf, sizeof buf, "[SoS %s, %s%s, lag %.0f ms]", s.fixed ? "1.1.0" : "1.0.0",
		s.dodgeVersion == 0 ? "no dodge mod" : (s.dodgeVersion == 100 ? "Simple Dodge 1.0.0" : "Simple Dodge 1.1.0"),
		s.dodgeVersion == 0 ? "" : (!s.sprintTap ? " sprint tap off" : (s.doubleTap ? " double tap" : " single tap")), s.gameLag * 1000.0);
	return buf;
}

int main()
{
	// 1. The owner's report reproduced: 1.0.0 with the game's held time a frame behind - a hold never sprints.
	for (int v : { 100, 110 })
	{
		Setup s;
		s.fixed = false;
		s.dodgeVersion = v;
		s.gameLag = 1.0 / 60.0;
		const auto w = Run(s, { { 0.0, 1.0 } });
		Check(w.sprintFrames == 0 && w.gamePresses == 0, Name(s) + " hold while moving: no sprint, the game never gets a press (the report)");
	}

	// 1b. The same 1.0.0 chain with the two clocks in step: the hold sprints - the failure is the race, not the chain itself.
	for (int v : { 100, 110 })
	{
		Setup s;
		s.fixed = false;
		s.dodgeVersion = v;
		const auto w = Run(s, { { 0.0, 1.0 } });
		Check(w.sprintFrames > 40, Name(s) + " hold while moving, clocks in step: sprints (1.0.0 worked only when it won the race)");
	}

	// 2. 1.1.0: every combination of dodge version, tap mode and clock skew.
	for (int v : { 100, 110 })
	{
		for (bool dbl : { false, true })
		{
			for (double lag : { 0.0, 1.0 / 60.0, 0.05, -0.05 })
			{
				Setup s;
				s.dodgeVersion = v;
				s.doubleTap = dbl;
				s.gameLag = lag;
				const std::string n = Name(s);
				{
					const auto w = Run(s, { { 0.0, 1.0 } });
					Check(w.sprintFrames > 40 && w.dodges == 0 && w.crouchChanges == 0, n + " hold while moving: sprints, no dodge");
				}
				{
					Setup st = s;
					st.moving = false;
					const auto w = Run(st, { { 0.0, 1.0 } });
					Check(w.crouchChanges == 1 && w.sneaking && w.dodges == 0 && w.sprintFrames == 0, n + " hold while still: crouches once, no dodge");
				}
				{
					Setup st = s;
					st.moving = false;
					st.sneaking = true;
					const auto w = Run(st, { { 0.0, 1.0 } });
					Check(w.crouchChanges == 1 && !w.sneaking && w.dodges == 0, n + " hold while still and crouched: stands up, no dodge");
				}
				{
					Setup st = s;
					st.sneaking = true;
					const auto w = Run(st, { { 0.0, 1.0 } });
					Check(w.crouchChanges == 1 && !w.sneaking && w.sprintFrames > 40 && w.dodges == 0, n + " hold while moving and crouched: stands up and sprints");
				}
				{
					const auto w = Run(s, { { 0.0, 0.15 } });
					Check(w.dodges == (dbl ? 0 : 1) && w.sprintFrames == 0 && w.crouchChanges == 0,
						n + (dbl ? " one tap: no dodge (double tap wanted), no sprint" : " one tap: one dodge, no sprint"));
				}
				{
					const auto w = Run(s, { { 0.0, 0.12 }, { 0.25, 0.37 } });
					Check(w.dodges == (dbl ? 1 : 2) && w.sprintFrames == 0 && w.crouchChanges == 0,
						n + (dbl ? " double tap: one dodge, no sprint" : " two taps: two dodges, no sprint"));
				}
				{
					const auto w = Run(s, { { 0.0, 0.12 }, { 0.25, 1.25 } });
					Check(w.dodges == (dbl ? 0 : 1) && w.sprintFrames > 40,
						n + (dbl ? " tap then hold: no dodge, the hold sprints" : " tap then hold: one dodge, then the hold sprints"));
				}
				{
					Setup st = s;
					st.moving = false;
					const auto w = Run(st, { { 0.0, 0.28 } });
					Check(w.dodges == (dbl ? 0 : 1) && w.crouchChanges == 0, n + " a press just under the hold time: a tap, not a crouch");
				}
			}
		}
	}

	// 3. Simple Dodge's Sprint tap off, and no dodge mod: nothing refereed, a hold sprints, a tap does nothing.
	for (int v : { 0, 100, 110 })
	{
		Setup s;
		s.dodgeVersion = v;
		s.sprintTap = false;
		const auto w = Run(s, { { 0.0, 1.0 } });
		Check(w.sprintFrames > 40 && w.dodges == 0, Name(s) + " hold while moving: sprints");
		const auto t = Run(s, { { 0.0, 0.15 } });
		Check(t.dodges == 0 && t.crouchChanges == 0, Name(s) + " tap: nothing");
		Setup st = s;
		st.moving = false;
		const auto c = Run(st, { { 0.0, 1.0 } });
		Check(c.crouchChanges == 1 && c.sneaking, Name(s) + " hold while still: crouches");
	}

	// 4. A longer dodge window (0.50 s) with the automatic hold following it.
	for (int v : { 100, 110 })
	{
		Setup s;
		s.dodgeVersion = v;
		s.window = 0.50f;
		s.holdSecs = 0.50f;
		s.gameLag = 1.0 / 60.0;
		const auto w = Run(s, { { 0.0, 1.2 } });
		Check(w.sprintFrames > 30 && w.dodges == 0, Name(s) + " window 0.50: hold sprints");
		const auto t = Run(s, { { 0.0, 0.40 } });
		Check(t.dodges == 1 && t.sprintFrames == 0, Name(s) + " window 0.50: a 0.40 s press dodges");
	}

	// 5. PressReferee itself.
	using sprint::referee::Show;
	Check(Show(0.0f, true, false, false, 0.2f).heldSecs == 0.2f && !Show(0.0f, true, true, true, 0.2f).clearFlagFirst, "no window: shown as it came");
	Check(Show(0.3f, true, false, false, 0.0f).heldSecs == 0.0f, "first down keeps held 0 (a press)");
	Check(Show(0.3f, true, false, false, 0.35f).heldSecs < 0.3f, "undecided down past the window: shown under it");
	Check(Show(0.3f, true, true, true, 0.25f).heldSecs >= 0.3f && Show(0.3f, true, true, true, 0.25f).clearFlagFirst, "sprint start: at the window, flag cleared");
	Check(!Show(0.3f, true, true, false, 0.4f).clearFlagFirst, "later sprint frames: flag left on");
	Check(Show(0.3f, false, false, false, 0.31f).heldSecs < 0.3f && Show(0.3f, false, false, false, 0.31f).heldSecs > 0.0f, "release of a tap: under the window, above 0");
	Check(Show(0.3f, false, true, false, 0.28f).heldSecs >= 0.3f, "release of a hold: at the window or over");

	std::printf("%d of %d passed\n", g_n - g_fail, g_n);
	return g_fail == 0 ? 0 : 1;
}
