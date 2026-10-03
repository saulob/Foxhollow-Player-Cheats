#include "player_cheats.h"

#include <string.h>

/* Health the god-mode guards hold while the game subtracts damage. */
#define GUARD_HEALTH INT8_MAX
/* playerProcessMessages applies the param of these messages as damage. */
#define MSG_PLAYER_DAMAGE_FIRST 0x60003
#define MSG_PLAYER_DAMAGE_LAST 0x60005
#define FAST_MOVEMENT_SCALE 2.0f

typedef struct Symbol {
  const char* name;
  void** address;
} Symbol;

typedef void (*HookFn)(void);

typedef struct Hook {
  const char* name;
  HookFn replacement;
  void** original;
  void* target;
} Hook;

typedef struct CheatGroup {
  Cheat cheat;
  const Symbol* symbols;
  int symbolCount;
  Hook* hooks;
  int hookCount;
} CheatGroup;

typedef struct HealthGuard {
  PlayerStatus* status;
  int8_t health;
} HealthGuard;

typedef struct MagicGuard {
  PlayerStatus* status;
  int16_t magic;
} MagicGuard;

PlayerCheatsGame game;

static GameObject* sMessageObj;
static GameObject* sHitObj;
static HealthGuard sHitGuard;
static GameObject* sMainMoveObj;

static void (*origPlayerAddHealth)(GameObject*, int);
static void (*origPlayerProcessMessages)(GameObject*, PlayerState*, PlayerState*);
static int (*origObjMsgPopNative)(GameObject*, uint32_t*, uintptr_t*, uintptr_t*);
static void (*origPlayerProcessHitResponse)(GameObject*, PlayerState*, PlayerState*);
static int (*origObjHitsGetPriorityHitWithPosition)(GameObject*, GameObject**, int*, uint32_t*, float*, float*, float*);
static void (*origPlayerUpdateSurfaceResponse)(GameObject*, PlayerState*, PlayerState*, float);
static void (*origPlayerAddRemoveMagic)(GameObject*, int);
static void (*origPlayerCastSpell)(GameObject*, PlayerState*, int);
static int (*origPlayerStateClimbWall)(GameObject*, PlayerState*);
static int (*origPlayerStateOnLadder)(GameObject*, PlayerState*);
static void (*origPlayerUpdateVelocityFromMotion)(GameObject*, PlayerState*, void*, float);
static int (*origObjMove)(GameObject*, float, float, float);

/* God Mode. The game subtracts damage inline in several places, so the hooks
   below either zero the damage before it reaches the subtraction or hold the
   health high while it happens and put the real value back afterwards. A guard
   whose health reaches zero was an instant-death hit and is left alone. */

static void guard_health(HealthGuard* guard, PlayerStatus* status, int8_t health) {
  guard->status = status;
  guard->health = health;
  status->health = GUARD_HEALTH;
}

static void release_health(HealthGuard* guard) {
  if (guard->status != NULL && guard->status->health > 0) {
    guard->status->health = guard->health;
  }
  guard->status = NULL;
}

static void hookPlayerAddHealth(GameObject* obj, int amount) {
  if (gCheatOn[CHEAT_GOD_MODE] && amount < 0) {
    amount = 0;
  }
  origPlayerAddHealth(obj, amount);
}

static void hookPlayerProcessMessages(GameObject* obj, PlayerState* inner, PlayerState* state) {
  GameObject* outer = sMessageObj;

  sMessageObj = obj;
  origPlayerProcessMessages(obj, inner, state);
  sMessageObj = outer;
}

static int hookObjMsgPopNative(GameObject* obj, uint32_t* message, uintptr_t* sender, uintptr_t* param) {
  int popped = origObjMsgPopNative(obj, message, sender, param);

  if (popped != 0 && obj == sMessageObj && gCheatOn[CHEAT_GOD_MODE] && message != NULL && param != NULL &&
      *message >= MSG_PLAYER_DAMAGE_FIRST && *message <= MSG_PLAYER_DAMAGE_LAST && (int)*param > 0) {
    *param = 0;
  }
  return popped;
}

static void hookPlayerProcessHitResponse(GameObject* obj, PlayerState* inner, PlayerState* state) {
  sHitObj = obj;
  origPlayerProcessHitResponse(obj, inner, state);
  sHitObj = NULL;
  release_health(&sHitGuard);
}

/* Called once by playerProcessHitResponse to fetch the hit. The damage becomes
   1 so the hit reaction still plays, and the health is guarded so it cannot
   drop. A priority 1 hit replaces the damage with the current health, which
   keeps instant-death hits (voids, crushers) fatal exactly as before. */
static int hookObjHitsGetPriorityHitWithPosition(GameObject* obj, GameObject** hitObj, int* sphere, uint32_t* damage,
                                                 float* x, float* y, float* z) {
  int priority = origObjHitsGetPriorityHitWithPosition(obj, hitObj, sphere, damage, x, y, z);
  PlayerStatus* status;

  if (priority != 0 && obj == sHitObj && sHitGuard.status == NULL && gCheatOn[CHEAT_GOD_MODE] && damage != NULL &&
      (int)*damage > 0 && (status = playerStatusOf(obj)) != NULL) {
    /* playerProcessHitResponse raises non-positive health to 1 at this point. */
    guard_health(&sHitGuard, status, status->health > 0 ? status->health : 1);
    *damage = 1;
  }
  return priority;
}

/* Sinking surfaces drain one health per tick inside this function. */
static void hookPlayerUpdateSurfaceResponse(GameObject* obj, PlayerState* state, PlayerState* cfg, float dt) {
  HealthGuard guard = {NULL, 0};
  PlayerStatus* status = gCheatOn[CHEAT_GOD_MODE] ? playerStatusOf(obj) : NULL;

  if (status != NULL && status->health > 0) {
    guard_health(&guard, status, status->health);
  }
  origPlayerUpdateSurfaceResponse(obj, state, cfg, dt);
  release_health(&guard);
}

/* Infinite Magic. Spells subtract their cost inline inside the player state
   functions, so each of them is wrapped and any magic it spent is refunded
   when it returns. */

static MagicGuard guard_magic(GameObject* obj) {
  MagicGuard guard = {NULL, 0};

  if (gCheatOn[CHEAT_INFINITE_MAGIC] && (guard.status = playerStatusOf(obj)) != NULL) {
    guard.magic = guard.status->magic;
  }
  return guard;
}

static void release_magic(MagicGuard guard) {
  if (guard.status != NULL && guard.status->magic < guard.magic) {
    guard.status->magic = guard.magic < guard.status->maxMagic ? guard.magic : guard.status->maxMagic;
  }
}

static void hookPlayerAddRemoveMagic(GameObject* obj, int amount) {
  if (gCheatOn[CHEAT_INFINITE_MAGIC] && amount < 0) {
    amount = 0;
  }
  origPlayerAddRemoveMagic(obj, amount);
}

static void hookPlayerCastSpell(GameObject* obj, PlayerState* state, int spell) {
  MagicGuard guard = guard_magic(obj);

  origPlayerCastSpell(obj, state, spell);
  release_magic(guard);
}

#define MAGIC_STATE_HOOK(name)                                          \
  static int (*orig_##name)(GameObject*, PlayerState*, float);          \
  static int hook_##name(GameObject* obj, PlayerState* state, float fv) { \
    MagicGuard guard = guard_magic(obj);                                \
    int result = orig_##name(obj, state, fv);                           \
    release_magic(guard);                                               \
    return result;                                                      \
  }

MAGIC_STATE_HOOK(playerStateSuperQuake)
MAGIC_STATE_HOOK(playerStateStaffBoost)
MAGIC_STATE_HOOK(playerState30)
MAGIC_STATE_HOOK(playerStateShootFireball)
MAGIC_STATE_HOOK(playerStateTryCastSpell)
MAGIC_STATE_HOOK(playerStateAimStaff)

/* Fast Movement. Climbing and ladder states return 0 only on the path that
   stores the new climb speed; the early exits leave it untouched. */

static int scale_climb_speed(GameObject* obj, PlayerState* state, int result) {
  if (result == 0 && gCheatOn[CHEAT_FAST_MOVEMENT] && state != NULL && obj == game.Obj_GetPlayerObject()) {
    state->moveSpeed *= FAST_MOVEMENT_SCALE;
  }
  return result;
}

static int hookPlayerStateClimbWall(GameObject* obj, PlayerState* state) {
  return scale_climb_speed(obj, state, origPlayerStateClimbWall(obj, state));
}

static int hookPlayerStateOnLadder(GameObject* obj, PlayerState* state) {
  return scale_climb_speed(obj, state, origPlayerStateOnLadder(obj, state));
}

/* playerUpdate calls this right before the objMove that applies the player's
   horizontal velocity, so the next objMove for the player is that one. The
   shared controller moves the player earlier in the frame and stays 1x. */
static void hookPlayerUpdateVelocityFromMotion(GameObject* obj, PlayerState* state, void* baddie, float dt) {
  origPlayerUpdateVelocityFromMotion(obj, state, baddie, dt);
  sMainMoveObj = gCheatOn[CHEAT_FAST_MOVEMENT] && obj == game.Obj_GetPlayerObject() ? obj : NULL;
}

static int hookObjMove(GameObject* obj, float dx, float dy, float dz) {
  if (obj != NULL && obj == sMainMoveObj) {
    sMainMoveObj = NULL;
    if (gCheatOn[CHEAT_FAST_MOVEMENT]) {
      dx *= FAST_MOVEMENT_SCALE;
      dz *= FAST_MOVEMENT_SCALE;
    }
  }
  return origObjMove(obj, dx, dy, dz);
}

static const Symbol kCoreSymbols[] = {
    {"getGameState", (void**)&game.getGameState},
    {"getCurUiDll", (void**)&game.getCurUiDll},
    {"getSaveGameLoadStatus", (void**)&game.getSaveGameLoadStatus},
    {"Obj_GetPlayerObject", (void**)&game.Obj_GetPlayerObject},
    {"getArwing", (void**)&game.getArwing},
};

static const Symbol kGodModeSymbols[] = {
    {"playerAddHealth", (void**)&game.playerAddHealth},
};

static const Symbol kInfiniteMagicSymbols[] = {
    {"playerAddRemoveMagic", (void**)&game.playerAddRemoveMagic},
};

static const Symbol kTrickySymbols[] = {
    {"getTrickyObject", (void**)&game.getTrickyObject},
    {"SaveGame_getTrickyStats", (void**)&game.SaveGame_getTrickyStats},
};

static Hook sGodModeHooks[] = {
    {"playerAddHealth", (HookFn)hookPlayerAddHealth, (void**)&origPlayerAddHealth, NULL},
    {"playerProcessMessages", (HookFn)hookPlayerProcessMessages, (void**)&origPlayerProcessMessages, NULL},
    {"ObjMsg_PopNative", (HookFn)hookObjMsgPopNative, (void**)&origObjMsgPopNative, NULL},
    {"playerProcessHitResponse", (HookFn)hookPlayerProcessHitResponse, (void**)&origPlayerProcessHitResponse, NULL},
    {"ObjHits_GetPriorityHitWithPosition", (HookFn)hookObjHitsGetPriorityHitWithPosition,
     (void**)&origObjHitsGetPriorityHitWithPosition, NULL},
    {"playerUpdateSurfaceResponse", (HookFn)hookPlayerUpdateSurfaceResponse, (void**)&origPlayerUpdateSurfaceResponse,
     NULL},
};

#define MAGIC_STATE_HOOK_ENTRY(name) {#name, (HookFn)hook_##name, (void**)&orig_##name, NULL}

static Hook sInfiniteMagicHooks[] = {
    {"playerAddRemoveMagic", (HookFn)hookPlayerAddRemoveMagic, (void**)&origPlayerAddRemoveMagic, NULL},
    {"playerCastSpell", (HookFn)hookPlayerCastSpell, (void**)&origPlayerCastSpell, NULL},
    MAGIC_STATE_HOOK_ENTRY(playerStateSuperQuake),
    MAGIC_STATE_HOOK_ENTRY(playerStateStaffBoost),
    MAGIC_STATE_HOOK_ENTRY(playerState30),
    MAGIC_STATE_HOOK_ENTRY(playerStateShootFireball),
    MAGIC_STATE_HOOK_ENTRY(playerStateTryCastSpell),
    MAGIC_STATE_HOOK_ENTRY(playerStateAimStaff),
};

static Hook sFastMovementHooks[] = {
    {"playerStateClimbWall", (HookFn)hookPlayerStateClimbWall, (void**)&origPlayerStateClimbWall, NULL},
    {"playerStateOnLadder", (HookFn)hookPlayerStateOnLadder, (void**)&origPlayerStateOnLadder, NULL},
    {"playerUpdateVelocityFromMotion", (HookFn)hookPlayerUpdateVelocityFromMotion,
     (void**)&origPlayerUpdateVelocityFromMotion, NULL},
    {"objMove", (HookFn)hookObjMove, (void**)&origObjMove, NULL},
};

#define COUNT_OF(array) ((int)(sizeof(array) / sizeof((array)[0])))

static CheatGroup sGroups[] = {
    {CHEAT_GOD_MODE, kGodModeSymbols, COUNT_OF(kGodModeSymbols), sGodModeHooks, COUNT_OF(sGodModeHooks)},
    {CHEAT_INFINITE_MAGIC, kInfiniteMagicSymbols, COUNT_OF(kInfiniteMagicSymbols), sInfiniteMagicHooks,
     COUNT_OF(sInfiniteMagicHooks)},
    {CHEAT_FAST_MOVEMENT, NULL, 0, sFastMovementHooks, COUNT_OF(sFastMovementHooks)},
    {CHEAT_INFINITE_TRICKY_ENERGY, kTrickySymbols, COUNT_OF(kTrickySymbols), NULL, 0},
};

static int resolve_symbols(FhMod* mod, const FhModHost* host, const Symbol* symbols, int count, FhLogLevel level) {
  int ok = 1;
  int i;

  for (i = 0; i < count; i++) {
    *symbols[i].address = host->symbolAddress(mod, symbols[i].name);
    if (*symbols[i].address == NULL) {
      modLog(level, "could not resolve %s", symbols[i].name);
      ok = 0;
    }
  }
  return ok;
}

static void remove_hooks(FhMod* mod, const FhModHost* host, Hook* hooks, int count) {
  int i;

  for (i = count - 1; i >= 0; i--) {
    if (hooks[i].target != NULL) {
      host->hookRemove(mod, hooks[i].target);
      hooks[i].target = NULL;
      *hooks[i].original = NULL;
    }
  }
}

static int install_hooks(FhMod* mod, const FhModHost* host, Hook* hooks, int count, FhLogLevel level) {
  int i;

  for (i = 0; i < count; i++) {
    void* target = host->symbolAddress(mod, hooks[i].name);

    if (target == NULL) {
      modLog(level, "could not resolve %s", hooks[i].name);
      remove_hooks(mod, host, hooks, count);
      return 0;
    }
    if (host->hookInstall(mod, target, (void*)hooks[i].replacement, hooks[i].original) != FH_MOD_OK) {
      modLog(level, "could not hook %s (no patch pad, or another mod already hooked it)", hooks[i].name);
      remove_hooks(mod, host, hooks, count);
      return 0;
    }
    hooks[i].target = target;
  }
  return 1;
}

int playerHooksInstall(FhMod* mod, const FhModHost* host) {
  int available = 0;
  int i;

  if (!resolve_symbols(mod, host, kCoreSymbols, COUNT_OF(kCoreSymbols), FH_LOG_ERROR)) {
    return 0;
  }
  for (i = 0; i < COUNT_OF(sGroups); i++) {
    CheatGroup* group = &sGroups[i];
    int ok = resolve_symbols(mod, host, group->symbols, group->symbolCount, FH_LOG_WARN) &&
             install_hooks(mod, host, group->hooks, group->hookCount, FH_LOG_WARN);

    gCheatAvailable[group->cheat] = (unsigned char)ok;
    if (ok) {
      available++;
    } else {
      modLog(FH_LOG_WARN, "%s unavailable: required game symbols or hooks are missing", gCheatNames[group->cheat]);
    }
  }
  return available > 0;
}

void playerHooksRemove(FhMod* mod, const FhModHost* host) {
  int i;

  for (i = COUNT_OF(sGroups) - 1; i >= 0; i--) {
    remove_hooks(mod, host, sGroups[i].hooks, sGroups[i].hookCount);
    gCheatAvailable[sGroups[i].cheat] = 0;
  }
  sMessageObj = NULL;
  sHitObj = NULL;
  sHitGuard.status = NULL;
  sMainMoveObj = NULL;
  memset(&game, 0, sizeof(game));
}
