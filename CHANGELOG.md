# SprintOrSneakForController - changelog

Rule 61: this mod's own history, kept beside the code it describes.

## 1.0.0 - 2026-11-07 - untested

### Added
- First release (the owner's design, 2026-11-07). On a controller the Sprint button does both jobs: standing still, holding Sprint toggles crouch (standing to sneaking, sneaking to standing); moving, holding Sprint sprints only while held, and crouched it stands you up first; one hold changes crouch once at most. A tap is left to other mods (TK Dodge AIO's sprint tap); with that tap on, a hold starts after its tap time. Hooks the game's SprintHandler after every plugin's kDataLoaded so it is outermost, chains Walk or Run for Controller and TK Dodge AIO. Keyboard and horseback left to the game. No settings. DevBench tool sos.control (state, press/release driving). Both lines: SE 1.5.97 / AE 1.6.1170 and 1.7.99 / 1.7.104.
