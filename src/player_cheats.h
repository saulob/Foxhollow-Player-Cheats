#ifndef PLAYER_CHEATS_H_
#define PLAYER_CHEATS_H_

#include <stddef.h>
#include <stdint.h>

#include "foxhollow_mod_api.h"

/* Partial native 64-bit layouts of the game records the cheats touch, the same
   on every supported Foxhollow target. The offsets match what
   playerGetCurHealth, playerGetMaxMagic and playerState41 read and write in
   the Foxhollow build. */
typedef struct GameObject {
  uint8_t pad000[0x100];
  void* extra;
} GameObject;

typedef struct PlayerStatus {
  int8_t health;
  int8_t maxHealth;
  uint8_t pad02[0x02];
  int16_t magic;
  int16_t maxMagic;
  uint8_t pad08[0x04];
} PlayerStatus;

typedef struct PlayerState {
  uint8_t pad000[0x2E8];
  float moveSpeed;
  uint8_t pad2EC[0xC4];
  PlayerStatus* playerStatus;
} PlayerState;

typedef struct TrickyStats {
  uint8_t energy;
  uint8_t maxEnergy;
  uint8_t pad02[0x02];
} TrickyStats;

_Static_assert(offsetof(GameObject, extra) == 0x100, "GameObject.extra");
_Static_assert(offsetof(PlayerStatus, maxHealth) == 0x01, "PlayerStatus.maxHealth");
_Static_assert(offsetof(PlayerStatus, magic) == 0x04, "PlayerStatus.magic");
_Static_assert(offsetof(PlayerStatus, maxMagic) == 0x06, "PlayerStatus.maxMagic");
_Static_assert(sizeof(PlayerStatus) == 0x0C, "PlayerStatus");
_Static_assert(offsetof(PlayerState, moveSpeed) == 0x2E8, "PlayerState.baddie.moveSpeed");
_Static_assert(offsetof(PlayerState, playerStatus) == 0x3B0, "PlayerState.playerStatus");
_Static_assert(sizeof(TrickyStats) == 0x04, "TrickyStats");

typedef struct PlayerCheatsGame {
  int (*getGameState)(void);
  int (*getCurUiDll)(void);
  int (*getSaveGameLoadStatus)(void);
  GameObject* (*Obj_GetPlayerObject)(void);
  GameObject* (*getArwing)(void);
  GameObject* (*getTrickyObject)(void);
  TrickyStats* (*SaveGame_getTrickyStats)(void);
  void (*playerAddHealth)(GameObject* obj, int amount);
  void (*playerAddRemoveMagic)(GameObject* obj, int amount);
} PlayerCheatsGame;

/* In key order: each cheat's index plus one is its number key. */
typedef enum Cheat {
  CHEAT_GOD_MODE,
  CHEAT_FAST_MOVEMENT,
  CHEAT_LADDER_SPEED,
  CHEAT_INFINITE_MAGIC,
  CHEAT_INFINITE_TRICKY_ENERGY,
  CHEAT_COUNT
} Cheat;

/* gCheatOn[CHEAT_LADDER_SPEED] holds one of these; the other cheats are 0 or 1. */
typedef enum LadderSpeedMode {
  LADDER_SPEED_OFF,
  LADDER_SPEED_2X,
  LADDER_SPEED_4X,
  LADDER_SPEED_MODES
} LadderSpeedMode;

extern PlayerCheatsGame game;
extern const char* const gCheatNames[CHEAT_COUNT];
extern unsigned char gCheatOn[CHEAT_COUNT];
extern unsigned char gCheatAvailable[CHEAT_COUNT];

static inline PlayerStatus* playerStatusOf(GameObject* obj) {
  if (obj == NULL || obj->extra == NULL) {
    return NULL;
  }
  return ((PlayerState*)obj->extra)->playerStatus;
}

void modLog(FhLogLevel level, const char* format, ...);

int playerHooksInstall(FhMod* mod, const FhModHost* host);
void playerHooksRemove(FhMod* mod, const FhModHost* host);

void playerCheatsUpdate(const int pressed[CHEAT_COUNT]);
void playerCheatsReset(void);

#endif
