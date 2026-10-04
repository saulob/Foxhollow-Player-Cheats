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

- Keys 1-5 work on both the number row and the numeric keypad (Numpad 1-5, with Num Lock on). Press once to enable a cheat and press again to disable it (Ladder Speed cycles through its steps instead). Holding either version of a key does not toggle repeatedly.
- Ladder Speed has three steps: the first press sets 2x, the second 4x, and the third returns to normal speed. 3 and Numpad 3 are one control, so holding one and pressing the other does not skip a step.
- Keys work during gameplay while the game window is focused.
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

Compatible with [Foxhollow Fly Mode](https://github.com/saulob/Foxhollow-Fly-Mode); both mods can be installed and used together.

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
```

Restart the game after installing.

## Platform support

- Windows x64

Other Foxhollow platforms are not supported by this mod yet.

## Repository

https://github.com/saulob/Foxhollow-Player-Cheats

## License

[MIT](LICENSE)
