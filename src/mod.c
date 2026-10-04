#include "player_cheats.h"
#include "platform_input.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

FH_MOD_EXPORT int fh_mod_initialize(FhMod* mod, const FhModHost* host) {
  int i;

  if (!host || host->abiVersion != FH_MOD_ABI_VERSION || host->structSize < sizeof(FhModHost)) return FH_MOD_ERROR;
  if (!host->log || !host->symbolAddress || !host->hookInstall || !host->hookRemove) return FH_MOD_ERROR;
  H = host;
  M = mod;
  if (!platformInputInitialize(mod, host)) {
    modLog(FH_LOG_ERROR, "disabled: keyboard input is unavailable");
    return FH_MOD_ERROR;
  }
  if (!playerHooksInstall(mod, host)) {
    playerHooksRemove(mod, host);
    platformInputShutdown();
    modLog(FH_LOG_ERROR, "disabled: required host symbols or hooks are unavailable");
    return FH_MOD_ERROR;
  }
  /* A key already held while the game starts is not a press. */
  for (i = 0; i < CHEAT_COUNT; i++) {
    sKeyDown[i] = platformCheatKeyDown(i);
  }
  modLog(FH_LOG_INFO,
         "v1.1.0 loaded (1 God Mode, 2 Fast Movement, 3 Ladder Speed 2x/4x, 4 Infinite Magic, "
         "5 Infinite Tricky Energy)");
  return FH_MOD_OK;
}

FH_MOD_EXPORT void fh_mod_update(FhMod* mod) {
  int pressed[CHEAT_COUNT];
  int focused = platformInputActive();
  int i;
  (void)mod;

  /* Key state is tracked while unfocused too, so a key already held when the
     game regains focus is not seen as a new press, and a key pressed while the
     game is in the background is dropped, not queued. Both keys of a cheat
     share one state, so it toggles once until both are released. */
  for (i = 0; i < CHEAT_COUNT; i++) {
    int down = platformCheatKeyDown(i);

    pressed[i] = focused && down && !sKeyDown[i];
    sKeyDown[i] = down;
  }
  playerCheatsUpdate(pressed);
}

FH_MOD_EXPORT void fh_mod_shutdown(FhMod* mod) {
  (void)mod;
  if (H && M) playerHooksRemove(M, H);
  playerCheatsReset();
  platformInputShutdown();
  memset(sKeyDown, 0, sizeof(sKeyDown));
  H = 0;
  M = 0;
}
