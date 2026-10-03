#include "player_cheats.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Number-row keys, indexed by Cheat: 1 God Mode, 2 Fast Movement, 3 Infinite Magic, 4 Infinite Tricky Energy. */
static const int kCheatKeys[CHEAT_COUNT] = {'1', '3', '2', '4'};

static const FhModHost* H;
static FhMod* M;
static int sKeyDown[CHEAT_COUNT];

void modLog(FhLogLevel level, const char* format, ...) {
  char message[256];
  int prefix;
  va_list args;

  if (!H || !H->log || !M) return;
  prefix = snprintf(message, sizeof(message), "[Player Cheats] ");
  va_start(args, format);
  vsnprintf(message + prefix, sizeof(message) - (size_t)prefix, format, args);
  va_end(args);
  H->log(M, level, message);
}

static int game_window_focused(void) {
  HWND window = GetForegroundWindow();
  DWORD processId = 0;

  if (window == NULL) return 0;
  GetWindowThreadProcessId(window, &processId);
  return processId == GetCurrentProcessId();
}

static int key_down(int key) {
  return (GetAsyncKeyState(key) & 0x8000) != 0;
}

FH_MOD_EXPORT int fh_mod_initialize(FhMod* mod, const FhModHost* host) {
  if (!host || host->abiVersion != FH_MOD_ABI_VERSION || host->structSize < sizeof(FhModHost)) return FH_MOD_ERROR;
  if (!host->log || !host->symbolAddress || !host->hookInstall || !host->hookRemove) return FH_MOD_ERROR;
  H = host;
  M = mod;
  if (!playerHooksInstall(mod, host)) {
    playerHooksRemove(mod, host);
    modLog(FH_LOG_ERROR, "disabled: required host symbols or hooks are unavailable");
    return FH_MOD_ERROR;
  }
  modLog(FH_LOG_INFO, "v1.0.0 loaded (1 God Mode, 2 Fast Movement, 3 Infinite Magic, 4 Infinite Tricky Energy)");
  return FH_MOD_OK;
}

FH_MOD_EXPORT void fh_mod_update(FhMod* mod) {
  int pressed[CHEAT_COUNT];
  int focused = game_window_focused();
  int i;
  (void)mod;

  /* Key state is tracked while unfocused too, so a key already held when the
     game regains focus is not seen as a new press. */
  for (i = 0; i < CHEAT_COUNT; i++) {
    int down = key_down(kCheatKeys[i]);

    pressed[i] = focused && down && !sKeyDown[i];
    sKeyDown[i] = down;
  }
  playerCheatsUpdate(pressed);
}

FH_MOD_EXPORT void fh_mod_shutdown(FhMod* mod) {
  (void)mod;
  if (H && M) playerHooksRemove(M, H);
  playerCheatsReset();
  memset(sKeyDown, 0, sizeof(sKeyDown));
  H = 0;
  M = 0;
}
