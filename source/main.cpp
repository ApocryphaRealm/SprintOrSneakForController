// Sprint or Sneak for Controller - own code, GPL-3.0-or-later. On a controller the Sprint button holds to sprint and, standing still,
// holds to crouch or stand; a tap is left free. No in-game page; the delays are in its INI. See the head of source/Sprint.cpp.
#include "PCH.h"

#include "DevBenchTool.h"
#include "Sprint.h"

#include "utils/AddressLibraryGuard.h"
#include "utils/Logger.h"

namespace
{
	void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg) { return; }
		switch (a_msg->type)
		{
		case SKSE::MessagingInterface::kPostLoad:
			DevBenchTool::Init(false);
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			sprint::Install();
			DevBenchTool::Init(true);
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			// The INI, and TK Dodge AIO's page, may have changed since the last game.
			sprint::ReadSettings();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::log::init("SprintOrSneakForController");
	// Address Library pre-check, BEFORE SKSE::Init, which opens the Address Library itself: a missing file gets a message
	// naming it and the plugin loads inert instead of CommonLibSSE-NG's bare failure line.
	if (!AddressLibraryGuard::Guard("Sprint or Sneak for Controller")) { return true; }
	SKSE::Init(a_skse);

	// [Debug] uLogLevel: 0 trace ... 2 info (the default: each hold's decision, no per-press lines) ... 6 off (rule 14).
	// Read here, before any setting is read, so the first lines already obey it.
	const auto ini = (std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / "SprintOrSneakForController.ini").string();
	const auto level = static_cast<int>(::GetPrivateProfileIntA("Debug", "uLogLevel", 2, ini.c_str()));
	SKSE::log::set_level(static_cast<spdlog::level::level_enum>(std::clamp(level, 0, 6)), spdlog::level::info);
	logger::info("Sprint or Sneak for Controller {} loading", SKSE::PluginDeclaration::GetSingleton()->GetVersion().string("."));

	auto* messaging = SKSE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(MessageHandler))
	{
		logger::error("could not register the SKSE message listener; the Sprint button stays the game's");
		return true;
	}
	return true;
}
