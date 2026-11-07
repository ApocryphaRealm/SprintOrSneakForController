#include "PCH.h"

#include "DevBenchTool.h"

#include "DevBench/DevBenchAPI.h"
#include "Sprint.h"
#include "utils/Logger.h"

#include <format>
#include <string>
#include <string_view>

// DevBench tool "sos.control" (rules 31 and 64): the Sprint button's live state, and a press of it with no hands on the pad. A driven
// press goes to the game's SprintHandler through its vtable, so the whole chain (this mod, TK Dodge, Walk or Run, the game) sees it.
// English only: only a tool reads this (rule 66).
namespace DevBenchTool
{
	namespace
	{
		// The value of a top-level JSON member, as text: a quoted string or a bare word. Empty when absent.
		std::string Get(std::string_view a_json, const char* a_name)
		{
			const std::string key = std::format("\"{}\"", a_name);
			auto pos = a_json.find(key);
			if (pos == std::string_view::npos) { return {}; }
			pos = a_json.find(':', pos + key.size());
			if (pos == std::string_view::npos) { return {}; }
			++pos;
			while (pos < a_json.size() && (a_json[pos] == ' ' || a_json[pos] == '\t')) { ++pos; }
			if (pos >= a_json.size()) { return {}; }
			if (a_json[pos] == '"')
			{
				const auto close = a_json.find('"', pos + 1);
				return close == std::string_view::npos ? std::string{} : std::string(a_json.substr(pos + 1, close - pos - 1));
			}
			std::string out;
			while (pos < a_json.size() && a_json[pos] != ',' && a_json[pos] != '}' && a_json[pos] != ' ') { out += a_json[pos++]; }
			return out;
		}

		std::string Escaped(std::string_view a_text)
		{
			std::string out;
			for (const char c : a_text)
			{
				if (c == '"' || c == '\\') { out += '\\'; }
				out += c;
			}
			return out;
		}

		void ControlTool(void*, const char* a_args, void* a_sink, DevBenchAPI::WriteFn a_write)
		{
			const std::string_view args = a_args ? a_args : "";
			const std::string op = Get(args, "op");
			const std::string moving = Get(args, "moving");
			std::string ok = "true";
			if (op == "press") { sprint::DrivePress(!moving.empty(), moving == "true" || moving == "1" ? 1 : 0); }
			else if (op == "release") { sprint::DriveRelease(); }
			else if (op == "reread") { sprint::ReadDodgeTap(); }
			else if (!op.empty() && op != "state") { ok = "false,\"error\":\"op is state, press, release or reread\""; }

			const auto s = sprint::GetStats();
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* state = player ? player->AsActorState() : nullptr;
			auto* controls = RE::PlayerControls::GetSingleton();
			const float mx = controls ? controls->data.moveInputVec.x : 0.0f;
			const float my = controls ? controls->data.moveInputVec.y : 0.0f;
			const std::string out = std::format(
				R"({{"ok":{},"op":"{}","installed":{},"outermost":{},"holdSecs":{:.2f},"dodgeIniFound":{},"dodgeSprintTap":{},"dodgeTapSecs":{:.2f},"events":{},"presses":{},"taps":{},"crouchOn":{},"crouchOff":{},"sprintHolds":{},"standAndSprint":{},"passedMounted":{},"held":{},"heldSecs":{:.2f},"mode":"{}","crouchChanged":{},"driving":{},"last":"{}","sneaking":{},"sprinting":{},"sprintFlag":{},"swimming":{},"mounted":{},"moveInput":[{:.2f},{:.2f}]}})",
				ok, op.empty() ? "state" : op, s.installed, s.outermost, s.holdSecs, s.dodgeTapRead, s.dodgeSprintTap, s.dodgeTapSecs, s.events, s.presses, s.taps,
				s.crouchOn, s.crouchOff, s.sprintHolds, s.standAndSprint, s.passedMounted, s.held, s.heldSecs, s.mode, s.crouchChanged, s.driving, Escaped(s.last),
				state && state->IsSneaking(), state && state->IsSprinting(), player && player->GetPlayerRuntimeData().playerFlags.isSprinting,
				state && state->IsSwimming(), player && player->IsOnMount(), mx, my);
			a_write(a_sink, out.c_str());
		}
	}

	void Init(bool a_lastAttempt)
	{
		static bool registered = false;
		if (registered) { return; }
		DevBenchAPI::IDevBenchInterface001* devBench = DevBenchAPI::GetDevBenchInterface001();
		if (!devBench)
		{
			if (a_lastAttempt) { logger::info("DevBench not detected; skipping the \"sos.control\" tool"); }
			return;
		}
		constexpr const char* descriptor =
			"{"
			"\"description\":\"Sprint or Sneak for Controller live state and driver. op=state (default): hook installed and outermost, the hold time, TK Dodge AIO's sprint tap as read, counts of presses, taps, crouches, stands and sprints, the current hold's mode, and the player's sneaking, sprinting, swimming and move input. op=press [moving=true|false]: presses the gamepad Sprint button through the game's SprintHandler and holds it every frame until op=release; moving overrides the stick for this mod's still-or-moving choice only (a real sprint still needs the stick). op=reread: reads TK Dodge AIO's settings again.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"op\":{\"type\":\"string\"},\"moving\":{\"type\":\"boolean\"}}},"
			"\"readOnly\":false"
			"}";
		if (devBench->RegisterTool("sos.control", descriptor, &ControlTool, nullptr))
		{
			logger::info("registered \"sos.control\" with DevBench (build {})", devBench->GetBuildNumber());
			registered = true;
		}
	}
}
