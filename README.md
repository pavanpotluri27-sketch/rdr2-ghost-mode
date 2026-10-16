# Ghost Mode for Red Dead Redemption 2

Walk through RDR2 story mode as a see-through, glowing ghost. Townsfolk scream and flee, your punch throws people through the air, and a left-click spirit blast sends everyone in front of you flying. Your horse turns ghostly too.

**Download:** [Nexus Mods](https://www.nexusmods.com/reddeadredemption2/mods/10874) · [Releases](../../releases)

## What it does
- See-through body with a soft blue glow and spirit sparkles
- White outfit goes on automatically (optional)
- Strangers panic and run; gang members in camp stare and gasp instead
- **F** ghost punch: the person in front of you flies
- **Left click** (empty hands) spirit blast: everyone in front of you flies
- Ghost horse, big / huge / giant size, ghost tails, hover
- You can't be hurt and the law ignores you while you're a ghost

## Keys (changeable in `GhostMode.ini`)
| Key | Action |
|---|---|
| F10 | Ghost on / off |
| Page Up | Size: normal > big > huge > giant |
| Page Down | See-through: faint > normal > solid |
| Insert | Ghost tails on / off |
| End | Hover / come down |
| F | Ghost punch |
| Left click (no weapon) | Spirit blast |

## Requirements
1. [Script Hook RDR2](http://www.dev-c.com/rdr2/scripthookrdr2/) (Alexander Blade), or Script Hook RDR2 V2. You need `ScriptHookRDR2.dll`.
2. An ASI loader: `dinput8.dll`, which comes with Script Hook RDR2.

Tested on game version 1.0.1491.50. Story mode only.

## Install
1. Put `ScriptHookRDR2.dll` and `dinput8.dll` next to `RDR2.exe`.
2. Copy `GhostMode.asi` and `GhostMode.ini` into the same folder.
3. Load a story save and press **F10**.

## Known issues
- **Online Content Unlocker** stops Script Hook from seeing the people around you, so nobody flees and the blast hits nobody. Disable it while using Ghost Mode.
- Wagon drivers don't fly from the blast.
- Turning the ghost off doesn't change your outfit back.

## Build from source
One C file and no dependencies. Build it with [zig](https://ziglang.org/) (`pip install ziglang` works too):
```
zig cc -target x86_64-windows-gnu -shared -O2 -s -o GhostMode.asi src/ghostmode.c
```
Or use `build.bat`.

## Bugs / requests
Open an [issue](../../issues) and attach `GhostMode.log` (next to the .asi).
**I build custom mods on request** (powers, characters, scenes, tools for YouTube creators). Ask on my [discussion board](https://github.com/pavanpotluri27-sketch/pavanpotluri27-sketch/discussions).

## More from me
Moon Glow sky mod on [Nexus](https://www.nexusmods.com/reddeadredemption2/mods/11046) · videos on [YouTube](https://www.youtube.com/@Bounty_Bandit_Gaming)

## Credits
Alexander Blade (Script Hook RDR2) · alloc8or (native database) · femga (rdr3_discoveries)

## License
MIT. See [LICENSE](LICENSE).
