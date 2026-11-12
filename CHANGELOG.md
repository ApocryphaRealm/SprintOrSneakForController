# SprintOrSneakForController - changelog

Rule 61: this mod's own history, kept beside the code it describes.

## 1.1.1 - 2026-11-12 - untested

### Fixed
- The HUD's sneak eye no longer stays on screen after this mod stands you up (Nexus report, 2026-11-12: "a persistent sneak eye icon even after I stop sneaking. Installing Contextual Crosshair fixed it"). The player did stand up; only the HUD was not told. The eye is shown and hidden only by a kCrosshairSneak HUD message from the game's stealth update, which the player's per-frame update runs only while sneaking, and which the game's own sneak changes run once more after the change; this mod's stand-up (the sneak setter's own writes, so Sneak-button listeners see no press) skipped that last run. After a stand-up, still or moving, the HUD now gets the message the game's update sends with sneaking off (show off, level 0), made and posted with the same two calls the game's sender uses (include/StealthMeter.h). Crouching, sprinting, taps, the dodge referee and Take a Peek are unchanged. sos.control state shows eyeHides.

### Added
- Offline tests: tests/test_stealth_meter.cpp (16 cases, run by tests/run-tests.ps1 with the 152 press-referee cases) - a model of the game's stealth meter reproduces the stuck eye with 1.1.0's stand-up and shows it gone with 1.1.1's, with the game's own Sneak, mixed rounds and one message per stand-up; tests/check-game-hud.py checks the game facts the fix rests on against the 1.5.97 and 1.7.104 code dumps (the message's layout in StealthMeter's ProcessMessage on both, its only senders, the per-frame update's sneaking gate).

## 1.1.0 - 2026-11-12 - untested

### Fixed
- With a dodge mod's Sprint-tap dodge on (Simple Dodge, TK Dodge AIO), holding Sprint on a controller now sprints (the owner, 2026-11-12: it would only sprint after a double press). Both mods held the same press on two clocks: when this mod decided the hold a frame before the game's held time reached the dodge window, the dodge mod saw the sprint flag already on and never handed the game the press that starts a sprint. This mod's clock now decides and the dodge mod is shown the answer through the held time it reads (include/PressReferee.h): under its window until the press is a hold, at or over it from then, the sprint flag cleared on the event the sprint starts; the event's own value is put back afterwards. A tap stays the dodge's (single or double tap), a hold sprints or crouches, never both.

### Added
- Offline tests (tests/run-tests.ps1, 152 cases): hold, still hold, crouched hold, tap, double press, tap-then-hold and near-threshold presses against copies of Simple Dodge 1.0.0's and 1.1.0's Sprint hook, single and double tap, with the game's held time in step, behind and ahead; and the 1.0.0 failure reproduced.
- Simple Dodge recognised like TK Dodge AIO: its DLL (SimpleDodge.dll) and SKSE/Plugins/Simple Dodge.ini are read for bUseSprintKey, fSprintingPressDuration, bUseDoubleTap and uDodgeKey; the automatic hold (fHoldSeconds = 0) follows the longer tap time of the dodge mods whose DLL is loaded with their Sprint tap on. A dodge mod's INI without its DLL loaded no longer counts. The load log names which dodge mod's tap time was taken; a warning when a dodge key is the controller's Sprint button. sos.control state shows dodgeMod, refereeSecs and refereed.

## 1.0.0 - 2026-11-07 - working

### Added
- [Debug] uLogLevel in the INI (2 = info by default: each hold's decision is logged, not every press).
- Take a Peek compatibility (the owner, 2026-11-07): while Take A Peek - New Stealth Mechanic.esp is loaded and bTakeAPeekDoors=1 (INI, default on), a Sprint hold made crouched and still with the crosshair on a closed door in reach (Take a Peek's own door test) changes nothing, so the peek is Take a Peek's; elsewhere the hold works as usual. Logged at debug; sos.control state shows takeAPeekLoaded, takeAPeekDoors and leftToPeek.
- SKSE/Plugins/SprintOrSneakForController.ini (the owner, 2026-11-07: no in-game page, an INI for the delays): fHoldSeconds (default 0 = automatic: 0.30 s, or TK Dodge AIO's tap time when its sprint tap is on and longer; any other value 0.05-2.00 used as is, with a warning in the log when shorter than TK Dodge's tap time) and fReleaseGapSeconds (default 0.25, how long without an event before a hold counts as let go). Read at data load, every game load and DevBench sos.control op=reread; one log line names the values; sos.control state shows holdSetting, holdSecs and releaseGapSecs.
- First release (the owner's design, 2026-11-07). On a controller the Sprint button does both jobs: standing still, holding Sprint toggles crouch (standing to sneaking, sneaking to standing); moving, holding Sprint sprints only while held, and crouched it stands you up first; one hold changes crouch once at most. A tap is left to other mods (TK Dodge AIO's sprint tap); with that tap on, a hold starts after its tap time. Hooks the game's SprintHandler after every plugin's kDataLoaded so it is outermost, chains Walk or Run for Controller and TK Dodge AIO. Keyboard and horseback left to the game. No settings. DevBench tool sos.control (state, press/release driving). Both lines: SE 1.5.97 / AE 1.6.1170 and 1.7.99 / 1.7.104.

### Fixed
- Skyrim 1.7: the Sprint button now reaches the mod (1.7 moved the input handler's button function), and a stick just let go no longer counts as moving.