#define _GNU_SOURCE

#include "platform_input.h"
#include "player_cheats.h"

#include <dlfcn.h>
#include <stdbool.h>
#include <stddef.h>

/* SDL_Scancode values from Foxhollow's SDL3 (SDL_scancode.h). */
#define SDL3_SCANCODE_1 30
#define SDL3_SCANCODE_2 31
#define SDL3_SCANCODE_3 32
#define SDL3_SCANCODE_4 33
#define SDL3_SCANCODE_5 34
#define SDL3_SCANCODE_KP_1 89
#define SDL3_SCANCODE_KP_2 90
#define SDL3_SCANCODE_KP_3 91
#define SDL3_SCANCODE_KP_4 92
#define SDL3_SCANCODE_KP_5 93

/* Scancodes indexed by Cheat: 1 God Mode, 2 Fast Movement, 3 Ladder Speed, 4 Infinite Magic, 5 Infinite Tricky Energy.
   Each cheat accepts its number-row key or the matching numpad key. */
static const int kCheatScancodes[CHEAT_COUNT] = {SDL3_SCANCODE_1, SDL3_SCANCODE_2, SDL3_SCANCODE_3, SDL3_SCANCODE_4,
                                                 SDL3_SCANCODE_5};
static const int kCheatKeypadScancodes[CHEAT_COUNT] = {SDL3_SCANCODE_KP_1, SDL3_SCANCODE_KP_2, SDL3_SCANCODE_KP_3,
                                                       SDL3_SCANCODE_KP_4, SDL3_SCANCODE_KP_5};

typedef const bool* (*SdlGetKeyboardStateFn)(int* numkeys);
typedef void* (*SdlGetKeyboardFocusFn)(void);

static SdlGetKeyboardStateFn sGetKeyboardState;
static SdlGetKeyboardFocusFn sGetKeyboardFocus;

static void* resolve_sdl_symbol(FhMod* mod, const FhModHost* host, const char* name) {
  void* address = dlsym(RTLD_DEFAULT, name);

  if (address == NULL) {
    address = host->symbolAddress(mod, name);
  }
  if (address == NULL) {
    modLog(FH_LOG_ERROR, "could not resolve %s", name);
  }
  return address;
}

int platformInputInitialize(FhMod* mod, const FhModHost* host) {
  const bool* keys;
  int count = 0;

  sGetKeyboardState = (SdlGetKeyboardStateFn)resolve_sdl_symbol(mod, host, "SDL_GetKeyboardState");
  sGetKeyboardFocus = (SdlGetKeyboardFocusFn)resolve_sdl_symbol(mod, host, "SDL_GetKeyboardFocus");
  if (sGetKeyboardState == NULL || sGetKeyboardFocus == NULL) {
    platformInputShutdown();
    return 0;
  }
  /* Numpad 5 is the highest scancode Player Cheats reads. */
  keys = sGetKeyboardState(&count);
  if (keys == NULL || count <= SDL3_SCANCODE_KP_5) {
    modLog(FH_LOG_ERROR, "SDL keyboard state is unavailable");
    platformInputShutdown();
    return 0;
  }
  return 1;
}

void platformInputShutdown(void) {
  sGetKeyboardState = NULL;
  sGetKeyboardFocus = NULL;
}

int platformInputActive(void) {
  return sGetKeyboardFocus != NULL && sGetKeyboardFocus() != NULL;
}

static int key_down(const bool* keys, int count, int scancode) {
  return keys != NULL && scancode < count && keys[scancode];
}

/* Keys are read by physical position: the number-row 1-5 keys and numpad 1-5.
   Scancodes ignore Shift and Num Lock, so numpad 1-5 work with Num Lock on or
   off. Both keys of a cheat are one control: it is down while either key is
   down. SDL clears this state when the window loses focus and restores only
   modifier keys when it regains it, so a key held across a focus change reads
   as up until it is pressed again. */
int platformCheatKeyDown(int cheat) {
  const bool* keys;
  int count = 0;

  if (sGetKeyboardState == NULL || cheat < 0 || cheat >= CHEAT_COUNT) return 0;
  keys = sGetKeyboardState(&count);
  return key_down(keys, count, kCheatScancodes[cheat]) || key_down(keys, count, kCheatKeypadScancodes[cheat]);
}
