#include "player_cheats.h"

#include <string.h>

#define GAME_STATE_RUNNING 1
#define UI_DLL_GAMEPLAY 1
#define UI_DLL_FRONTEND_FIRST 2
#define UI_DLL_FRONTEND_LAST 7

const char* const gCheatNames[CHEAT_COUNT] = {"God Mode", "Fast Movement", "Ladder Speed", "Infinite Magic",
                                              "Infinite Tricky Energy"};
static const char* const kLadderSpeedModeNames[LADDER_SPEED_MODES] = {"disabled", "2x", "4x"};

unsigned char gCheatOn[CHEAT_COUNT];
unsigned char gCheatAvailable[CHEAT_COUNT];

static int sSessionActive;

static int gameplay_active(void) {
  return game.getGameState() == GAME_STATE_RUNNING && game.getCurUiDll() == UI_DLL_GAMEPLAY &&
         game.getSaveGameLoadStatus() == 0 && (game.Obj_GetPlayerObject() != NULL || game.getArwing() != NULL);
}

static int any_cheat_on(void) {
  int i;

  for (i = 0; i < CHEAT_COUNT; i++) {
    if (gCheatOn[i]) return 1;
  }
  return 0;
}

/* Cheats belong to the loaded save: they survive warps, loads and the Arwing,
   and are cleared once the game stops running or a front-end screen (title,
   save select) takes over. */
static int update_session(void) {
  int uiDll;

  if (gameplay_active()) {
    sSessionActive = 1;
    return 1;
  }
  uiDll = game.getCurUiDll();
  if (sSessionActive && (game.getGameState() != GAME_STATE_RUNNING ||
                         (uiDll >= UI_DLL_FRONTEND_FIRST && uiDll <= UI_DLL_FRONTEND_LAST))) {
    sSessionActive = 0;
    if (any_cheat_on()) {
      memset(gCheatOn, 0, sizeof(gCheatOn));
      modLog(FH_LOG_INFO, "Save session ended, all cheats disabled");
    }
  }
  return 0;
}

static void refill_health(void) {
  GameObject* player = game.Obj_GetPlayerObject();
  PlayerStatus* status = playerStatusOf(player);
  int missing;

  if (status == NULL) return;
  missing = status->maxHealth - status->health;
  if (status->health > 0 && missing > 0) {
    game.playerAddHealth(player, missing);
  }
}

static void refill_magic(void) {
  GameObject* player = game.Obj_GetPlayerObject();
  PlayerStatus* status = playerStatusOf(player);
  int missing;

  if (status == NULL) return;
  missing = status->maxMagic - status->magic;
  if (missing > 0) {
    game.playerAddRemoveMagic(player, missing);
  }
}

static void fill_tricky_energy(void) {
  TrickyStats* stats;

  if (game.getTrickyObject() == NULL) return;
  stats = game.SaveGame_getTrickyStats();
  if (stats != NULL && stats->energy < stats->maxEnergy) {
    stats->energy = stats->maxEnergy;
  }
}

static void toggle(Cheat cheat) {
  if (!gCheatAvailable[cheat]) {
    modLog(FH_LOG_WARN, "%s is unavailable in this Foxhollow build", gCheatNames[cheat]);
    return;
  }
  /* Ladder Speed cycles Off -> 2x -> 4x -> Off instead of toggling. */
  if (cheat == CHEAT_LADDER_SPEED) {
    gCheatOn[cheat] = (unsigned char)((gCheatOn[cheat] + 1) % LADDER_SPEED_MODES);
    modLog(FH_LOG_INFO, "%s %s", gCheatNames[cheat], kLadderSpeedModeNames[gCheatOn[cheat]]);
    return;
  }
  if (!gCheatOn[cheat]) {
    if (cheat == CHEAT_GOD_MODE) {
      refill_health();
    } else if (cheat == CHEAT_INFINITE_MAGIC) {
      refill_magic();
    }
  }
  gCheatOn[cheat] = !gCheatOn[cheat];
  modLog(FH_LOG_INFO, "%s %s", gCheatNames[cheat], gCheatOn[cheat] ? "enabled" : "disabled");
}

void playerCheatsUpdate(const int pressed[CHEAT_COUNT]) {
  int i;

  if (!update_session()) return;
  if (game.getArwing() == NULL) {
    for (i = 0; i < CHEAT_COUNT; i++) {
      if (pressed[i]) toggle((Cheat)i);
    }
  }
  if (gCheatOn[CHEAT_INFINITE_TRICKY_ENERGY]) {
    fill_tricky_energy();
  }
}

void playerCheatsReset(void) {
  memset(gCheatOn, 0, sizeof(gCheatOn));
  memset(gCheatAvailable, 0, sizeof(gCheatAvailable));
  sSessionActive = 0;
}
