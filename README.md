# Foxhollow Player - Cheats

A native mod that adds player cheats to Star Fox Adventures running through Foxhollow.

## Controls

| Key | Action |
| --- | --- |
| 1 | Toggle God Mode |
| 2 | Toggle Fast Movement 2x |
| 3 | Cycle Ladder Speed: 2x / 4x / Off |
| 4 | Toggle Infinite Magic |
| 5 | Toggle Infinite Tricky Energy |

- Keys 1-5 work on both the number row and the numeric keypad. On Windows, the numpad keys need Num Lock on. On Linux and macOS, the keys are recognized by their position on the keyboard, so numpad 1-5 work with Num Lock on or off.
- Press a key once to enable its cheat and press it again to disable it (Ladder Speed cycles through its steps instead). Holding a key does not toggle repeatedly; release it and press it again to toggle again. The number-row key and the numpad key for the same number count as one: pressing one while the other is held does not toggle again.
- Ladder Speed has three steps: the first press sets 2x, the second 4x, and the third returns to normal speed. 3 and Numpad 3 are one control, so holding one and pressing the other does not skip a step.
- Keys only work during gameplay while the Foxhollow window has keyboard focus. A key pressed while the window is in the background is ignored, not saved for later.
- All cheats start off and reset when you leave the current save (returning to the title screen, the save select or a soft reset). Warps, loading screens, shops and Arwing flights keep them enabled.
- Infinite Tricky Energy only acts while Tricky is present.
- There is no on-screen display. Every change is written to the Foxhollow log, for example `[Player Cheats] God Mode enabled`.

## Features

- **God Mode**: normal damage no longer lowers your health, and enabling it refills your health. Instant-death hazards stay fatal, so the game can still recover you from places you cannot leave.
- **Fast Movement 2x**: doubles normal movement speed, including running and swimming. Automatic/scripted jumps may also travel farther while enabled. It does not change climbing speed.
- **Ladder Speed 2x / 4x**: makes climbing ladders and climbable walls 2x or 4x faster, going up or down, from the moment you get onto a ladder. It does not change any other movement.
- **Infinite Magic**: spells and staff abilities cost no magic, and enabling it refills your magic.
- **Infinite Tricky Energy**: keeps Tricky's energy full.

Fast Movement and Ladder Speed are independent: turn on either one, or both for faster movement everywhere. God Mode, Fast Movement, Ladder Speed and Infinite Magic work for both Fox and Krystal.

## Compatibility

Compatible with [Foxhollow Fly Mode](https://github.com/saulob/Foxhollow-Fly-Mode) and [Foxhollow Noclip](https://github.com/saulob/Foxhollow-Noclip); the mods can be installed and used together.

## Installation

**Recommended:** install through the Foxhollow Launcher once the mod is published there.

**Manual:** place the extracted mod folder in the Foxhollow Launcher's `mods` folder, so it looks like this:

```
mods/
  com.saulob.cheats-player/
    mod.json
    lib/
      windows-amd64/
        mod.dll
      linux-amd64/
        mod.so
      linux-arm64/
        mod.so
      macos-x86_64/
        mod.so
      macos-arm64/
        mod.so
```

Foxhollow only loads the library in the folder that matches your system and ignores the others, so you only need the folder for your platform.

Restart the game after installing.

## Platform support

| Platform | Folder | Status |
| --- | --- | --- |
| Windows x64 | `windows-amd64` | Tested in game |
| Linux x86_64 | `linux-amd64` | Build validation by GitHub Actions pending, in-game testing pending |
| Linux ARM64 | `linux-arm64` | Build validation by GitHub Actions pending, in-game testing pending |
| macOS Apple Silicon | `macos-arm64` | Build validation by GitHub Actions pending, in-game testing pending |
| macOS Intel | `macos-x86_64` | Build validation by GitHub Actions pending, in-game testing pending |

Official Foxhollow builds are currently published for Windows x64, Linux x86_64 and macOS Apple Silicon. The Linux ARM64 and macOS Intel libraries are for Foxhollow builds you compile yourself. Windows on ARM is not supported.

On Linux and macOS, keys 1-5 are read from the keyboard state of Foxhollow's own SDL3 runtime, so the mod needs no extra libraries and behaves the same under X11 and Wayland. If the Foxhollow log shows `[Player Cheats] disabled: ...`, the mod could not find the game functions or keyboard input it needs and left the game unchanged. If only some game functions are missing, the log names the affected cheats as unavailable and the others keep working.

## Repository

https://github.com/saulob/Foxhollow-Player-Cheats

## License

[MIT](LICENSE)
