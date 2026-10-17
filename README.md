# Super Metroid Plus

A widescreen Super Metroid playtest built on a modified **bsnes-hd** emulator, with room-specific rendering work and selected arena, enemy, and projectile boundary adjustments.

**[Download builds 253 / 254](https://github.com/peanutsofdoom/supermetroidplus/releases/tag/v253.254)** · **[Report an issue](https://github.com/peanutsofdoom/supermetroidplus/issues)**

This is an emulator-based project, not a native decompilation, recompilation, or a ROM patch for other emulators. Windows x64 is the packaged runtime; Steam Deck uses Proton 10.0. The original game's slowdown can still occur. This is a public playtest, not a claim of complete compatibility.

## Bring your own ROM

No game ROM, personal save, or test checkpoint is distributed. Supply your own unmodified **Super Metroid (Japan, USA)** ROM. The expected unheadered ROM is 3,145,728 bytes with SHA-256:

```text
12b77c4bc9c1832cee8881244659065ee1d84c70c3d29e6eaf92e6798cc2ca72
```

The importer also accepts the matching ROM with a 512-byte copier header and removes that header from the imported copy. Modified ROMs and other revisions are not supported by this build.

## Windows

1. Download `Super-Metroid-Plus-Windows-x64-253-254.zip` from the release and extract the whole folder to a writable location.
2. Open **Import ROM.cmd** and select your ROM.
3. Open **Play.exe**. **Play With Coordinates.exe** starts with the diagnostic HUD visible. Press **F8** to toggle it in either version.
4. If needed, open **Configure Controls.exe**, then Settings → Input → Controller Port 1 → Gamepad.

Both launchers share saves in `saves/`. There is no play-time limit. The package's [START HERE](docs/START%20HERE.txt) contains full controls and setup instructions.

## Steam Deck

This release runs through **Proton 10.0**, not a native Linux executable.

1. In Desktop Mode, extract the full Windows ZIP into a permanent folder.
2. Put your matching unheadered ROM in `games/Super Metroid.sfc`. Alternatively, use `python3 tools/check_rom.py "/path/to/your/rom.sfc"` from the package folder to validate/import it.
3. Add **Play.exe** or **Play With Coordinates.exe** to Steam as a non-Steam game. Keep **Start In** set to the extracted folder.
4. In its Steam Properties → Compatibility, select **Proton 10.0**. Use a standard Gamepad Steam Input layout and launch from Gaming Mode.

Map F8 to an unused rear button if you want to toggle the HUD. The package also includes a Windows-to-Deck setup helper. Do not enable the optional Windows Capture Mode on Deck.

## Quiver Launcher

Public catalog submission is pending; this project is not yet listed in the community catalog.

You can add `peanutsofdoom/supermetroidplus` manually in Quiver, using `Super-Metroid-Plus-Windows-x64` as the release asset filter. A [personal app list](quiver-apps.json) is also provided. Select **Play.exe** after importing your ROM from the installed folder; do not select the executable inside `app/` directly.

On Linux, select the Windows build and configure Proton, or use the tested Steam setup above. A clean Quiver download/install check is tracked separately from the earlier game validation; direct Quiver game launching on every platform is not yet qualified.

Keep automatic updates off during this playtest. Back up saves and install new releases separately before moving your own progress across. Quiver's update behavior has not been qualified for preserving every project-specific file.

## What's in this playtest

- Widescreen rendering across the game, including work on the intro, Ceres, bosses, and ending.
- Selected gameplay changes that let widened arenas, enemies, and projectiles use the added horizontal space. This is not entirely identical to stock gameplay.
- Normal build **253** and Coordinates build **254**.
- A fix for the Blue Brinstar scanner causing widescreen to drop out at certain beam angles.

For the 253/254 scanner change, 1,850 targeted and regression frames retained matching native centers, guest state, audio, and checkpoints; 872 previous narrow fallbacks were removed. Twenty-five focused probes and both 8,472-frame production GUI comparisons passed. These are bounded tests, not exhaustive game coverage. See [known observations](docs/KNOWN%20OBSERVATIONS.txt).

## Reporting issues

Open a GitHub issue with the build number, a short description, and a short clip or screenshot. A visible Coordinates HUD helps identify the room, but is optional. The packaged **Create Report.cmd** / `tools/create_report.py` collects a small diagnostic report; review it before attaching. Nothing is uploaded automatically. Do not attach ROMs or the entire game folder. The package's older Discord references can be ignored; use this repository's issues for public reports.

## Source and license

This repository contains the corresponding 253/254 source snapshot. An identical source ZIP is included in the runtime download and attached separately to the release. See [README-SOURCE.txt](README-SOURCE.txt) for the Windows MinGW-w64 rebuild recipe. The shipped binaries were qualified; a fresh rebuild from this public snapshot has not been independently qualified, and compiler/profile differences can change binary hashes.

Based on [bsnes-hd by DerKoun](https://github.com/DerKoun/bsnes-hd), with the upstream bsnes and bundled-library credits and license notices retained in `development/bsnes-hd-gui/`. Project additions use GPL-3.0-or-later; see [LICENSE](LICENSE) and the notices in individual files. The runtime package also includes the MinGW-w64 runtime notice.

Unofficial fan project. Not affiliated with or endorsed by Nintendo. Super Metroid and its game assets belong to their respective rights holders; no game assets are supplied by this release.
