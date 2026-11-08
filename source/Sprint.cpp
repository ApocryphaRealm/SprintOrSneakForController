// Sprint or Sneak for Controller - own code, GPL-3.0-or-later.
//
// THE DESIGN (the owner, 2026-11-07; quoted in 2. Mod Types\to-do list\to-do-list.md, "New mod: sprint and sneak mechanics"). On a
// controller the Sprint button does both jobs:
//   - standing still, HOLD Sprint: crouch changes - standing to sneaking, sneaking to standing;
//   - moving, HOLD Sprint: sprint, only while the button is held. Crouched, it stands you up and sprints;
//   - one continuous hold changes crouch once at most; the next change needs a fresh press;
//   - a TAP does nothing here, so a dodge mod's sprint tap (TK Dodge AIO's bUseSprintKey) keeps working.
// No in-game page (the owner, 2026-11-07); the delays are in SKSE/Plugins/SprintOrSneakForController.ini. The keyboard's Sprint key and riding
// a horse are left wholly to the game.
//
// WHERE. The game's SprintHandler (PlayerControls's handler for the Sprint user event), slot 4 of its vtable, ProcessButton. The game
// calls it with a button event every frame the button is down and once more on the release, through the control map, so a button UVC
// moved Sprint to is the button this acts on, and no key is read here. In 1.5.97 the handler only flips PlayerCharacter's isSprinting
// flag on a press (read for Walk or Run for Controller, source/Motion.cpp there); the player's update then sprints while that flag is
// on and the actor moves forward with stamina, and clears it otherwise.
//
// THE CHAIN. Walk or Run for Controller and TK Dodge AIO also write slot 4, each calling the function it found there. This mod writes
// it ONE FRAME after kDataLoaded (an SKSE task), after every plugin's kDataLoaded handler, so it is the outermost: it sees each event
// first, untouched (TK Dodge sets heldDownSecs to 0 on an event it passes), and it has the last word on the sprint flag because it
// writes it after the inner chain returns. Every event still goes down the chain, except the repeats of a hold that changed crouch,
// so TK Dodge sees every tap (its dodge fires on the release) and Walk or Run sees the presses and releases it needs for its sprint
// on Walk. The press of a tap reaches the game's own handler, which turns the flag on; this mod turns it off again before the
// frame's update reads it. A hold is told from a tap by this mod's own clock, not heldDownSecs, so a plugin outside this one that
// rewrites heldDownSecs cannot make one press look like many.
//
// HOW LONG IS A HOLD. The INI's fHoldSeconds; at 0 (automatic, the default) kHoldSecs, or TK Dodge AIO's fSprintingPressDuration
// when its sprint tap is on and that is longer: a press
// TK Dodge still counts as a dodge tap must not also crouch or sprint.
//
// TAKE A PEEK (the owner, 2026-11-07: holding near a door would be both a peek and a crouch change). While Take A Peek - New Stealth
// Mechanic.esp is loaded and the INI's bTakeAPeekDoors is on, a hold made crouched and still with the crosshair on a closed door (Take
// a Peek's own test, in reach because the game sets the crosshair reference only then) changes nothing: the peek is Take a Peek's.
//
// CROUCH. The game's own sneak setter (1.5.97 id 36926) as Separate Swimming Controls replays its off branch: the iIsInSneak graph
// variable, ActorState's sneaking bit, and the SneakStart / SneakStop graph event. It is not reached through the SneakHandler, so a
// mod that listens to the Sneak button (TK Dodge's sneak-key dodge, Take a Peek's hold-Sneak-to-peek) sees no press from it.
#include "PCH.h"

#include "Sprint.h"

#include "utils/Logger.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <thread>

namespace sprint
{
	namespace
	{
		// The INI's defaults (dist\SKSE\Plugins\SprintOrSneakForController.ini ships the same values; rule 16).
		constexpr float kAutoHold = 0.0f;          // fHoldSeconds=0: automatic, kHoldSecs or TK Dodge AIO's tap time
		constexpr float kHoldSecs = 0.30f;         // the automatic hold without TK Dodge's sprint tap
		constexpr float kDefaultGapSecs = 0.25f;   // fReleaseGapSeconds: no event for this long, the button was let go unheard
		constexpr bool kDefaultTakeAPeekDoors = true;  // bTakeAPeekDoors: a crouched, still hold at a closed door is Take a Peek's
		constexpr std::string_view kTakeAPeekPlugin = "Take A Peek - New Stealth Mechanic.esp";
		constexpr float kMoveDeadzone = 0.15f;     // the left stick's push that counts as moving (moveInputVec length)

		enum class Mode : int
		{
			kNone,       // the button is up
			kUndecided,  // down, not yet a hold
			kSprint,     // a hold that sprints while it lasts
			kCrouch,     // a hold that changed crouch while still (it sprints only if it stood the player up and they then move)
		};

		struct Hold
		{
			Mode mode = Mode::kNone;
			std::int64_t pressedAt = 0;
			std::int64_t lastEventAt = 0;
			bool crouchChanged = false;
			bool tapPassed = false;  // the press went down the chain as an ordinary press
		};

		// Main thread only: the handler, the tasks and the menu sink's task touch it.
		Hold g_hold;

		std::atomic<bool> g_installed{ false };
		std::atomic<float> g_holdSecs{ kHoldSecs };       // in force
		std::atomic<float> g_holdSetting{ kAutoHold };    // as the INI has it
		std::atomic<std::int64_t> g_gapMs{ 250 };
		std::atomic<bool> g_dodgeRead{ false };
		std::atomic<bool> g_dodgeSprintTap{ false };
		std::atomic<float> g_dodgeTapSecs{ 0.0f };
		std::atomic<std::uint64_t> g_events{ 0 }, g_presses{ 0 }, g_taps{ 0 }, g_crouchOn{ 0 }, g_crouchOff{ 0 }, g_sprintHolds{ 0 }, g_standAndSprint{ 0 }, g_passedMounted{ 0 }, g_leftToPeek{ 0 };
		std::atomic<bool> g_takeAPeekLoaded{ false };
		std::atomic<bool> g_takeAPeekDoors{ true };  // bTakeAPeekDoors
		std::atomic<int> g_modeShown{ 0 };
		std::atomic<bool> g_heldShown{ false };
		std::atomic<float> g_heldSecsShown{ 0.0f };
		std::atomic<bool> g_crouchChangedShown{ false };
		std::mutex g_lastLock;
		std::string g_last = "nothing yet";

		// DevBench driving
		std::atomic<bool> g_driving{ false };
		std::atomic<int> g_forceMoving{ -1 };  // -1 the stick decides; 0 still; 1 moving
		std::atomic<std::uint64_t> g_driveId{ 0 };
		RE::ButtonEvent* g_driveEvent = nullptr;  // made once on the main thread, reused for every driven event
		std::int64_t g_drivePressedAt = 0;

		std::uintptr_t g_hookFn = 0;
		std::uintptr_t g_slot = 0;

		std::int64_t NowMs()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		}

		void SetLast(std::string a_text)
		{
			logger::debug("sprint: {}", a_text);
			std::scoped_lock l(g_lastLock);
			g_last = std::move(a_text);
		}

		const char* ModeName(Mode a_mode)
		{
			switch (a_mode)
			{
			case Mode::kUndecided: return "undecided";
			case Mode::kSprint: return "sprint";
			case Mode::kCrouch: return "crouch";
			default: return "none";
			}
		}

		void Publish(float a_heldSecs)
		{
			g_modeShown.store(static_cast<int>(g_hold.mode), std::memory_order_relaxed);
			g_heldShown.store(g_hold.mode != Mode::kNone, std::memory_order_relaxed);
			g_heldSecsShown.store(a_heldSecs, std::memory_order_relaxed);
			g_crouchChangedShown.store(g_hold.crouchChanged, std::memory_order_relaxed);
		}

		void SetSprintFlag(bool a_on)
		{
			if (auto* player = RE::PlayerCharacter::GetSingleton()) { player->GetPlayerRuntimeData().playerFlags.isSprinting = a_on; }
		}

		// The game's sneak setter, both branches (see the head). Main thread.
		void SetSneaking(bool a_on)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* state = player ? player->AsActorState() : nullptr;
			if (!state)
			{
				logger::warn("sprint: no player state; crouch not changed");
				return;
			}
			if (state->IsSneaking() == a_on) { return; }
			player->SetGraphVariableInt("iIsInSneak", a_on ? 1 : 0);
			state->actorState1.sneaking = a_on ? 1 : 0;
			const bool sent = player->NotifyAnimationGraph(a_on ? "SneakStart" : "SneakStop");
			logger::debug("sprint: sneaking {} (graph event {})", a_on ? "on" : "off", sent ? "taken" : "not taken");
		}

		bool Moving(const RE::PlayerControlsData* a_data)
		{
			const int forced = g_forceMoving.load(std::memory_order_relaxed);
			if (forced >= 0) { return forced == 1; }
			if (!a_data) { return false; }
			const auto& v = a_data->moveInputVec;
			const bool input = v.x * v.x + v.y * v.y > kMoveDeadzone * kMoveDeadzone;
#if RUNTIME_LINE == 17
			// On 1.7 moveInputVec keeps the last input batch's value, so a "forward" can outlive the stick's release; the
			// game's own per-frame direction bits on the player's ActorState settle it (Walk or Run for Controller 1.2.1).
			if (!input) { return false; }
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* state = player ? player->AsActorState() : nullptr;
			if (!state) { return true; }
			const auto& s1 = state->actorState1;
			return s1.movingBack || s1.movingForward || s1.movingRight || s1.movingLeft;
#else
			return input;
#endif
		}

		void EndHold(const char* a_why)
		{
			if (g_hold.mode == Mode::kSprint) { SetSprintFlag(false); }
			if (g_hold.mode != Mode::kNone) { logger::debug("sprint: hold ended ({}), {} after {} ms", a_why, ModeName(g_hold.mode), NowMs() - g_hold.pressedAt); }
			g_hold = Hold{};
			Publish(0.0f);
		}

		// Take a Peek's own test for a door it peeks through (Keyhole_Exit.psc, its Sneak-hold release): the crosshair reference,
		// which the game sets only within activation reach, has a Door base and an open state of 3 (closed). Its exceptions
		// formlist is not read here. Only while Take a Peek's plugin is loaded and bTakeAPeekDoors is on.
		bool PeekDoorAhead()
		{
			if (!g_takeAPeekLoaded.load(std::memory_order_relaxed) || !g_takeAPeekDoors.load(std::memory_order_relaxed)) { return false; }
			auto* pick = RE::CrosshairPickData::GetSingleton();
			if (!pick) { return false; }
			const auto ref = pick->target.get();
			if (!ref) { return false; }
			auto* base = ref->GetBaseObject();
			const bool door = base && base->Is(RE::FormType::Door);
			const bool closed = door && RE::BGSOpenCloseForm::GetOpenState(ref.get()) == RE::BGSOpenCloseForm::OPEN_STATE::kClosed;
			logger::debug("sprint: crosshair {:08X} {} {}", ref->GetFormID(), door ? "is a door" : "is not a door", door ? (closed ? "(closed: Take a Peek's)" : "(not closed)") : "");
			return closed;
		}

		// Past the hold time: what this hold does now. Called every held frame; each branch acts once.
		void Decide(RE::PlayerCharacter* a_player, const RE::PlayerControlsData* a_data)
		{
			auto* state = a_player->AsActorState();
			if (!state) { return; }
			const bool moving = Moving(a_data);
			const bool sneaking = state->IsSneaking();
			const bool swimming = state->IsSwimming();
			if (g_hold.mode == Mode::kSprint) { return; }
			if (moving)
			{
				if (!sneaking)
				{
					g_hold.mode = Mode::kSprint;
					g_sprintHolds.fetch_add(1, std::memory_order_relaxed);
					SetLast("hold while moving: sprinting until the button is let go");
				}
				else if (!g_hold.crouchChanged)
				{
					SetSneaking(false);
					g_hold.crouchChanged = true;
					g_hold.mode = Mode::kSprint;
					g_sprintHolds.fetch_add(1, std::memory_order_relaxed);
					g_standAndSprint.fetch_add(1, std::memory_order_relaxed);
					SetLast("hold while moving and crouched: stood up and sprinting");
				}
				// Crouched by this same hold and now moving: it stays crouched. The next press can stand it up.
				return;
			}
			if (g_hold.mode == Mode::kUndecided && !g_hold.crouchChanged && !swimming)
			{
				// Crouched and still at a door Take a Peek can peek through: the hold is Take a Peek's, so crouch is not changed
				// (the owner, 2026-11-07). The hold is spent: it changes nothing else, as a crouch change would not.
				if (sneaking && PeekDoorAhead())
				{
					g_hold.crouchChanged = true;
					g_hold.mode = Mode::kCrouch;
					g_leftToPeek.fetch_add(1, std::memory_order_relaxed);
					SetLast("hold while crouched and still at a closed door: left to Take a Peek, crouch unchanged");
					return;
				}
				SetSneaking(!sneaking);
				g_hold.crouchChanged = true;
				g_hold.mode = Mode::kCrouch;
				(sneaking ? g_crouchOff : g_crouchOn).fetch_add(1, std::memory_order_relaxed);
				SetLast(sneaking ? "hold while still: stood up" : "hold while still: crouched");
			}
		}

		struct SprintHandlerHook
		{
			static void ProcessButton(RE::SprintHandler* a_this, RE::ButtonEvent* a_event, RE::PlayerControlsData* a_data)
			{
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!a_event || !player || a_event->GetDevice() != RE::INPUT_DEVICE::kGamepad)
				{
					original(a_this, a_event, a_data);
					return;
				}
				if (player->IsOnMount())
				{
					// The horse's gallop is the game's: nothing here changes it.
					if (g_hold.mode != Mode::kNone) { EndHold("mounted"); }
					g_passedMounted.fetch_add(1, std::memory_order_relaxed);
					original(a_this, a_event, a_data);
					return;
				}
				g_events.fetch_add(1, std::memory_order_relaxed);
				const std::int64_t now = NowMs();
				const bool down = a_event->Value() != 0.0f;

				if (down)
				{
					// A new press: the button was up, or nothing was heard from it for longer than a frame takes.
					if (g_hold.mode == Mode::kNone || now - g_hold.lastEventAt > g_gapMs.load(std::memory_order_relaxed))
					{
						if (g_hold.mode != Mode::kNone) { EndHold("not heard from"); }
						g_hold.mode = Mode::kUndecided;
						g_hold.pressedAt = now;
						g_presses.fetch_add(1, std::memory_order_relaxed);
					}
					g_hold.lastEventAt = now;
					const float heldSecs = static_cast<float>(now - g_hold.pressedAt) / 1000.0f;
					if (heldSecs >= g_holdSecs.load(std::memory_order_relaxed)) { Decide(player, a_data); }
					Publish(heldSecs);
					// A hold that changed crouch keeps its repeats from the chain: none of them is a tap, and Walk or Run would
					// otherwise answer "run" to a sneaking player who starts moving with the button still down.
					if (g_hold.mode != Mode::kCrouch) { original(a_this, a_event, a_data); }
					SetSprintFlag(g_hold.mode == Mode::kSprint);
					return;
				}

				// The release.
				if (g_hold.mode == Mode::kUndecided)
				{
					g_taps.fetch_add(1, std::memory_order_relaxed);
					SetLast("tap: left to the other mods");
				}
				original(a_this, a_event, a_data);
				SetSprintFlag(false);
				EndHold("let go");
			}
			static inline REL::Relocation<decltype(ProcessButton)> original;
		};

		// A menu that opens during a hold takes the release; the hold ends with it.
		class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			static MenuSink* Get()
			{
				static MenuSink sink;
				return &sink;
			}
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && a_event->opening && g_heldShown.load(std::memory_order_relaxed))
				{
					if (auto* tasks = SKSE::GetTaskInterface()) { tasks->AddTask([] { EndHold("a menu opened"); }); }
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		void WriteHook()
		{
			if (g_installed) { return; }
			REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_SprintHandler[0] };
#if RUNTIME_LINE == 17
			// Skyrim 1.7 put two new functions at slots 4 and 5 of every player input handler's vtable, so ProcessButton
			// moved to slot 6; CommonLibSSE-NG 7.2 still names slot 4 (found in Walk or Run for Controller, 2026-11-08).
			constexpr std::size_t kSlot = 6;
#else
			constexpr std::size_t kSlot = 4;
#endif
			SprintHandlerHook::original = vtable.write_vfunc(kSlot, SprintHandlerHook::ProcessButton);
			g_slot = vtable.address() + kSlot * sizeof(std::uintptr_t);
			g_hookFn = reinterpret_cast<std::uintptr_t>(&SprintHandlerHook::ProcessButton);
			g_installed = true;
			logger::info("sprint: SprintHandler slot {} hooked (the function it held is called on: {:X})", kSlot, SprintHandlerHook::original.address());
			if (auto* ui = RE::UI::GetSingleton()) { ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuSink::Get()); }
		}

		bool ReadBool(const std::string& a_value)
		{
			std::string v;
			for (const char c : a_value) { v += static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
			return v == "true" || v == "1";
		}

		std::string Trim(std::string a_text)
		{
			const auto from = a_text.find_first_not_of(" \t\r\"");
			const auto to = a_text.find_last_not_of(" \t\r\"");
			return from == std::string::npos ? std::string{} : a_text.substr(from, to - from + 1);
		}

		float ReadFloat(const std::string& a_value, float a_fallback)
		{
			try { return std::stof(a_value); }
			catch (...) { return a_fallback; }
		}

		// key = value lines of an INI, sections ignored (the keys here are unique), ';' comments dropped.
		template <class Fn>
		void ReadIni(const std::filesystem::path& a_path, bool& a_found, Fn&& a_fn)
		{
			a_found = false;
			std::ifstream in{ a_path };
			if (!in) { return; }
			a_found = true;
			std::string line;
			while (std::getline(in, line))
			{
				if (const auto semi = line.find(';'); semi != std::string::npos) { line.resize(semi); }
				const auto eq = line.find('=');
				if (eq == std::string::npos) { continue; }
				a_fn(Trim(line.substr(0, eq)), Trim(line.substr(eq + 1)));
			}
		}

		// One frame of a driven hold: the event goes to the game's SprintHandler through its vtable, as the pad's would.
		void DriveFrame(std::uint64_t a_id, bool a_down)
		{
			if (g_driveId.load() != a_id) { return; }
			auto* controls = RE::PlayerControls::GetSingleton();
			auto* events = RE::UserEvents::GetSingleton();
			if (!controls || !controls->sprintHandler || !events) { return; }
			const std::int64_t now = NowMs();
			if (!g_driveEvent)
			{
				g_driveEvent = RE::ButtonEvent::Create(RE::INPUT_DEVICE::kGamepad, events->sprint, 0x0100, 1.0f, 0.0f);
				if (!g_driveEvent) { return; }
			}
			if (a_down && g_drivePressedAt == 0) { g_drivePressedAt = now; }
			const float held = g_drivePressedAt ? static_cast<float>(now - g_drivePressedAt) / 1000.0f : 0.0f;
			g_driveEvent->GetRuntimeData().value = a_down ? 1.0f : 0.0f;
			g_driveEvent->GetRuntimeData().heldDownSecs = a_down ? held : std::max(held, 0.01f);
			controls->sprintHandler->ProcessButton(g_driveEvent, &controls->data);
			if (!a_down) { g_drivePressedAt = 0; }
		}
	}

	void Install()
	{
		ReadSettings();
		if (auto* tasks = SKSE::GetTaskInterface())
		{
			tasks->AddTask([] { WriteHook(); });
			logger::info("sprint: the SprintHandler hook goes in on the first frame, after every plugin's kDataLoaded");
		}
		else
		{
			logger::warn("sprint: no task interface; hooking now, which may leave another mod's SprintHandler hook outside this one");
			WriteHook();
		}
	}

	void ReadSettings()
	{
		std::error_code ec;
		const auto plugins = std::filesystem::current_path(ec) / "Data" / "SKSE" / "Plugins";

		// This mod's own INI (the owner, 2026-11-07: no in-game page, an INI for the delays). A missing key keeps its default.
		bool ownFound = false;
		float setHold = kAutoHold;
		float gap = kDefaultGapSecs;
		bool peekDoors = kDefaultTakeAPeekDoors;
		ReadIni(plugins / "SprintOrSneakForController.ini", ownFound, [&](const std::string& a_key, const std::string& a_value) {
			if (a_key == "fHoldSeconds") { setHold = ReadFloat(a_value, kAutoHold); }
			else if (a_key == "fReleaseGapSeconds") { gap = ReadFloat(a_value, kDefaultGapSecs); }
			else if (a_key == "bTakeAPeekDoors") { peekDoors = ReadBool(a_value); }
		});
		g_takeAPeekDoors = peekDoors;
		auto* data = RE::TESDataHandler::GetSingleton();
		g_takeAPeekLoaded = data && data->LookupModByName(kTakeAPeekPlugin);
		setHold = setHold <= 0.0f ? kAutoHold : std::clamp(setHold, 0.05f, 2.0f);
		gap = std::clamp(gap, 0.05f, 2.0f);

		// TK Dodge AIO's sprint tap: the automatic hold follows its tap time.
		bool found = false, tap = false;
		float secs = 0.0f;
		ReadIni(plugins / "TK Dodge AIO.ini", found, [&](const std::string& a_key, const std::string& a_value) {
			if (a_key == "bUseSprintKey") { tap = ReadBool(a_value); }
			else if (a_key == "fSprintingPressDuration") { secs = ReadFloat(a_value, 0.0f); }
		});
		g_dodgeRead = found;
		g_dodgeSprintTap = tap;
		g_dodgeTapSecs = secs;

		const bool automatic = setHold == kAutoHold;
		const float hold = !automatic ? setHold : (tap ? std::clamp(std::max(kHoldSecs, secs), kHoldSecs, 1.5f) : kHoldSecs);
		g_holdSetting = setHold;
		g_holdSecs = hold;
		g_gapMs = static_cast<std::int64_t>(gap * 1000.0f + 0.5f);
		logger::info("sprint: settings {}: fHoldSeconds={:.2f} ({}), fReleaseGapSeconds={:.2f}, bTakeAPeekDoors={}; TK Dodge AIO {}; Take a Peek {}; a hold is {:.2f} s",
			ownFound ? "read" : "file not found, defaults", automatic ? 0.0f : setHold, automatic ? "automatic" : "set", gap, peekDoors ? 1 : 0,
			found ? (tap ? std::format("sprint tap on ({:.2f} s)", secs) : std::string("sprint tap off")) : std::string("not found"),
			g_takeAPeekLoaded.load() ? "loaded" : "not loaded", hold);
		if (!automatic && tap && setHold < secs)
		{
			logger::warn("sprint: fHoldSeconds {:.2f} is shorter than TK Dodge AIO's tap time {:.2f}: a press between them both dodges and crouches or sprints", setHold, secs);
		}
	}

	void DrivePress(bool a_forceMoving, int a_forceMovingValue)
	{
		g_forceMoving = a_forceMoving ? a_forceMovingValue : -1;
		const std::uint64_t id = g_driveId.fetch_add(1) + 1;
		g_driving = true;
		std::thread([id] {
			while (g_driving.load() && g_driveId.load() == id)
			{
				if (auto* tasks = SKSE::GetTaskInterface()) { tasks->AddTask([id] { DriveFrame(id, true); }); }
				std::this_thread::sleep_for(std::chrono::milliseconds(16));
			}
		}).detach();
		SetLast("DevBench: Sprint pressed");
	}

	void DriveRelease()
	{
		const std::uint64_t id = g_driveId.load();
		g_driving = false;
		if (auto* tasks = SKSE::GetTaskInterface())
		{
			tasks->AddTask([id] {
				DriveFrame(id, false);
				g_forceMoving = -1;
			});
		}
	}

	Stats GetStats()
	{
		Stats s;
		s.installed = g_installed.load();
		s.outermost = g_installed.load() && g_slot && *reinterpret_cast<const std::uintptr_t*>(g_slot) == g_hookFn;
		s.holdSecs = g_holdSecs.load();
		s.holdSetting = g_holdSetting.load();
		s.releaseGapSecs = static_cast<float>(g_gapMs.load()) / 1000.0f;
		s.takeAPeekLoaded = g_takeAPeekLoaded.load();
		s.takeAPeekDoors = g_takeAPeekDoors.load();
		s.leftToPeek = g_leftToPeek.load();
		s.dodgeTapRead = g_dodgeRead.load();
		s.dodgeSprintTap = g_dodgeSprintTap.load();
		s.dodgeTapSecs = g_dodgeTapSecs.load();
		s.events = g_events.load();
		s.presses = g_presses.load();
		s.taps = g_taps.load();
		s.crouchOn = g_crouchOn.load();
		s.crouchOff = g_crouchOff.load();
		s.sprintHolds = g_sprintHolds.load();
		s.standAndSprint = g_standAndSprint.load();
		s.passedMounted = g_passedMounted.load();
		s.held = g_heldShown.load();
		s.heldSecs = g_heldSecsShown.load();
		s.mode = ModeName(static_cast<Mode>(g_modeShown.load()));
		s.crouchChanged = g_crouchChangedShown.load();
		s.driving = g_driving.load();
		{
			std::scoped_lock l(g_lastLock);
			s.last = g_last;
		}
		return s;
	}
}
