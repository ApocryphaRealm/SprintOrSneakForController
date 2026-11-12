Sprint or Sneak for Controller
Version 1.0.0

On a controller, the Sprint button does both jobs.

- Standing still, HOLD Sprint: you crouch. Hold it again while still: you stand up.
- Moving, HOLD Sprint: you sprint, for as long as you hold it. Let go and the sprint ends.
- Crouched and moving, HOLD Sprint: you stand up and sprint.
- One hold changes crouch once at most; press again for the next change.
- A TAP of Sprint does nothing here, so a dodge mod that dodges on a Sprint tap (TK Dodge AIO's or
  Simple Dodge's Sprint-tap dodge, single or double tap) keeps working. With that tap on, a hold
  starts once its tap time has passed, and this mod tells the dodge mod which a press was, so a press
  is never both a dodge and a sprint or crouch.

Whatever button Sprint is mapped to is the button this works on. The keyboard's Sprint key and riding
a horse are left as the game has them. No plugin, no scripts; nothing is stored in your save.

SETTINGS
No in-game page. SKSE\Plugins\SprintOrSneakForController.ini holds the two delays, each explained in the
file: fHoldSeconds (how long a press must be held to count as a hold; 0 = automatic: 0.30 seconds, or TK
Dodge AIO's or Simple Dodge's tap time when its Sprint-tap dodge is on) and fReleaseGapSeconds, and
bTakeAPeekDoors (below).

TAKE A PEEK
With Take a Peek installed, a Sprint hold while you are crouched, standing still and looking at a closed door
in reach does not stand you up: the hold is left to Take a Peek, so you peek. Elsewhere the hold works as usual.
Turn it off with bTakeAPeekDoors = 0.

WORKS WITH
- TK Dodge AIO and Simple Dodge: tap Sprint to dodge, hold Sprint to sprint or crouch.
- Walk or Run for Controller: sprint works on its Walk setting as well.
- Unbind Vanilla Controls 1.6.0 or newer: its Controls page and controller graphic show that the
  Sprint button also crouches.

NOT COMPATIBLE WITH
Other mods that make Sprint hold-to-sprint (Classic Sprinting Redone, Hold to Sprint): use one.

REQUIRES
SKSE and Address Library for SKSE Plugins. Skyrim SE 1.5.97, AE 1.6.1170, or 1.7.99 / 1.7.104.

LICENCE
GPL-3.0-or-later. Source: https://github.com/ApocryphaRealm/SprintOrSneakForController
