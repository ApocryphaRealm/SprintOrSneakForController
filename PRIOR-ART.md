# Sprint or Sneak for Controller - prior art

Searched 2026-11-07, before building (the owner's rule: check precedent before a new mod). Searched on Nexus Mods (Skyrim
Special Edition: "hold to sprint", "sprint toggle", "hold to sneak", "sneak on hold", "sprint sneak controller", "crouch
controller") and on GitHub (CommonLibSSE projects hooking SprintHandler or SneakHandler; "hold to sprint skyrim", "sneak
toggle skse").

## What exists

| Mod | Where | What it does | Against this mod |
|---|---|---|---|
| Classic Sprinting Redone | Nexus 20166 | Hold-to-sprint for the Sprint key | Same hold-to-sprint idea; no crouch on the Sprint button. **Incompatible**: both own the Sprint handler's press. |
| Hold to Sprint | Nexus 77023 | Hold-to-sprint (SKSE) | Same as above. **Incompatible**. |
| Hold to sprint (older) | Nexus 12220 | Hold-to-sprint | Same as above. **Incompatible**. |
| Key Logic Expansion | Nexus 133785 | Tap / hold / double-tap logic on keys | Partial: can make keys behave on hold, but does not put crouch on the Sprint button by stillness. |
| Sneak on Hold | Nexus 165717 | Hold the Sneak button to sneak | Sneak button, not Sprint; no shared button. |
| Custom Controller Keymap (CCKM) | Nexus 18106 | Controller remapping | Remaps buttons; no hold logic of this kind. |
| Immersive Controller | Nexus 27734 | Controller scheme changes | No Sprint-button crouch. |
| Exit Sneak On Sprint (SprintCancelsSneak.dll) | Nexus | Sprinting while sneaking stands you up | One part of this design; compatible (the player is already standing when the sprint starts). |

GitHub: no CommonLibSSE project found that puts sneak on the Sprint handler; the hold-to-sprint plugins found write the same
SprintHandler slot this one does.

## Conclusion

Nothing does the shared-button design (still + hold Sprint toggles crouch, moving + hold sprints, a tap left free for dodge mods).
The hold-to-sprint mods above overlap with one half and are listed as incompatible on the mod's page.
