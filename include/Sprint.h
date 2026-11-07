#pragma once

#include <cstdint>
#include <string>

// The Sprint button's two jobs on a controller. See the head of source/Sprint.cpp.
namespace sprint
{
	void Install();      // at kDataLoaded; the vtable write itself runs one frame later, after every plugin's kDataLoaded
	void ReadSettings();  // at kDataLoaded, every load and DevBench op=reread: this mod's INI and TK Dodge AIO's sprint tap

	// DevBench driving (main thread work is queued from any thread): a press of the Sprint button through the game's own
	// SprintHandler, held until Release, so the whole handler chain sees it as the pad's.
	void DrivePress(bool a_forceMoving, int a_forceMovingValue);
	void DriveRelease();

	struct Stats
	{
		bool installed = false;
		bool outermost = false;          // the SprintHandler slot still points at this mod's function
		float holdSecs = 0.0f;           // in force: a press held this long is a hold; shorter is a tap
		float holdSetting = 0.0f;        // fHoldSeconds as read; 0 automatic
		float releaseGapSecs = 0.0f;     // fReleaseGapSeconds
		bool dodgeTapRead = false;       // TK Dodge AIO's settings were found
		bool dodgeSprintTap = false;     // its bUseSprintKey
		float dodgeTapSecs = 0.0f;       // its fSprintingPressDuration
		std::uint64_t events = 0;        // gamepad Sprint events seen
		std::uint64_t presses = 0;
		std::uint64_t taps = 0;          // released before holdSecs: left to the other mods
		std::uint64_t crouchOn = 0;
		std::uint64_t crouchOff = 0;     // stood up by a still hold
		std::uint64_t sprintHolds = 0;   // holds that became a sprint
		std::uint64_t standAndSprint = 0;  // of those, the ones that stood the player up first
		std::uint64_t passedMounted = 0;   // events left wholly to the game: on horseback
		bool held = false;               // the Sprint button is down now
		float heldSecs = 0.0f;
		std::string mode;                // "none", "undecided", "sprint", "crouch"
		bool crouchChanged = false;      // this hold has changed crouch (it will not again)
		bool driving = false;            // a DevBench press is held
		std::string last;                // the last thing this mod did, in words
	};
	Stats GetStats();
}
