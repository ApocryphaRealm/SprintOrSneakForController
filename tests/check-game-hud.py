"""check-game-hud.py - checks, against the game's own code, the facts include/StealthMeter.h rests on (1.1.1, the sneak eye
that stayed up after a stand-up). Reads the decrypted code dumps through the project's gamecode.py (D:\\Claude output\\.MD\\
scripts; set GAMECODE_DIR to use another copy). Exit 0 = every fact holds; 1 = one does not; 2 = the dumps are not on this
machine (nothing checked).

Facts:
  1.5.97  StealthMeter::ProcessMessage (vtable 268828 slot 2) takes HUDData type 0x18 (kCrosshairSneak), show at +0x40,
          level at +0x44.
  1.5.97  the only sender of that message (id 50728) writes type 0x18, show at +0x40 and level at +0x44, and is called only
          by the stealth update (id 39514) and the HUD reset (id 50718).
  1.5.97  the player's per-frame call of the stealth update (in id 39375) is gated by id 19632, which tests the sneaking bit
          (ActorState +0xB8, 0x200) - so nothing hides the eye after the bit is cleared directly.
  1.5.97  the game's own Sneak action (59) reaches the stealth update through id 39495.
  1.7.104 StealthMeter::ProcessMessage (vtable 215386 slot 2) takes the same type 0x18, show at +0x40, level at +0x44.
"""
import os
import re
import struct
import sys

sys.path.insert(0, os.environ.get("GAMECODE_DIR", r"D:\Claude output\.MD\scripts"))
try:
    import gamecode
    from capstone import CS_ARCH_X86, CS_MODE_64, Cs
except Exception as e:  # noqa: BLE001
    print(f"SKIP: gamecode.py / capstone not available ({e})")
    sys.exit(2)

IMAGE = 0x140000000
fails = 0


def check(ok, what):
    global fails
    print(("PASS " if ok else "FAIL ") + what)
    if not ok:
        fails += 1


def runtime(name):
    try:
        rt = gamecode.Runtime(name)
    except Exception as e:  # noqa: BLE001
        print(f"SKIP {name}: {e}")
        return None
    if rt.text is None:
        print(f"SKIP {name}: no code dump")
        return None
    return rt


def func_text(rt, rva, at_least=0):
    # the .pdata range, or at_least bytes where the compiler split the function (1.7.104's ProcessMessage ends its first
    # unwind range at +0x29, before the reads)
    f = rt.owner(rva)
    end = max(f[1] if f else rva + 0x200, rva + at_least)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    return [(i.address, i.mnemonic, i.op_str) for i in md.disasm(rt.read(rva, end - rva), rva)]


def slot(rt, vt_id, n):
    vt = rt.ids[vt_id]
    return struct.unpack("<Q", rt.read(vt + n * 8, 8))[0] - IMAGE


def has(ins, pattern):
    rx = re.compile(pattern)
    return any(rx.search(f"{m} {o}") for _, m, o in ins)


def calls(ins, target):
    return any(m == "call" and o == hex(target) for _, m, o in ins)


def message_reader(ins):
    return (has(ins, r"cmp dword ptr \[r\w+ \+ 0x10\], 0x18") and has(ins, r"byte ptr \[r\w+ \+ 0x40\]")
            and has(ins, r"dword ptr \[r\w+ \+ 0x44\]"))


checked = 0
se = runtime("1.5.97")
if se:
    checked += 1
    pm = func_text(se, slot(se, 268828, 2))
    check(message_reader(pm), "1.5.97 StealthMeter::ProcessMessage reads HUDData type 0x18, show +0x40, level +0x44")
    send = func_text(se, se.ids[50728])
    check(has(send, r"mov dword ptr \[rax \+ 0x10\], 0x18") and has(send, r"mov byte ptr \[rax \+ 0x40\]") and has(send, r"mov dword ptr \[rax \+ 0x44\]"),
          "1.5.97 id 50728 writes kCrosshairSneak (0x18), show +0x40, level +0x44")
    check(calls(send, se.ids[80061]) and calls(send, se.ids[13530]),
          "1.5.97 id 50728 makes it with UIMessageQueue::CreateUIMessageData (80061) and posts it with AddMessage (13530), the two calls source/Sprint.cpp makes")
    check(calls(func_text(se, se.ids[39514]), se.ids[50728]), "1.5.97 the stealth update (id 39514) sends it")
    check(calls(func_text(se, se.ids[50718]), se.ids[50728]), "1.5.97 the HUD reset (id 50718) sends it")
    gate = func_text(se, se.ids[19632])
    check(has(gate, r"mov edx, 0x200") and has(gate, r"add rcx, 0xb8"), "1.5.97 id 19632 tests the sneaking bit (ActorState +0xB8, 0x200)")
    upd = func_text(se, se.ids[39375])
    i_gate = next((k for k, (_, m, o) in enumerate(upd) if m == "call" and o == hex(se.ids[19632])), None)
    gated = i_gate is not None and any(m == "call" and o == hex(se.ids[39514]) for _, m, o in upd[i_gate:i_gate + 6]) \
        and upd[i_gate + 2][1] == "je"
    check(gated, "1.5.97 the per-frame call of the stealth update (id 39375) is skipped unless id 19632 says sneaking")
    check(calls(func_text(se, se.ids[39495]), se.ids[39514]), "1.5.97 the action handler (id 39495, reached for kActionSneak 59) calls the stealth update")
s17 = runtime("1.7.104")
if s17:
    checked += 1
    check(message_reader(func_text(s17, slot(s17, 215386, 2), 0x60)), "1.7.104 StealthMeter::ProcessMessage reads HUDData type 0x18, show +0x40, level +0x44")

if not checked:
    print("SKIP: no code dumps on this machine; nothing checked")
    sys.exit(2)
print(f"{'all facts hold' if not fails else str(fails) + ' fact(s) do not hold'}")
sys.exit(1 if fails else 0)
