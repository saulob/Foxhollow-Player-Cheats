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
  int needsSharedHooks;
} CheatGroup;

typedef struct HealthGuard {
  PlayerStatus* status;
  int8_t health;
} HealthGuard;

typedef struct MagicGuard {
  PlayerStatus* status;
  int16_t magic;
} MagicGuard;

/* The climb speed Ladder Speed last stored, and the game's own value it was
   made from. state is NULL when the stored speed is the game's own. */
typedef struct ClimbSpeed {
  PlayerState* state;
  float game;
  float scaled;
} ClimbSpeed;

PlayerCheatsGame game;

static GameObject* sMessageObj;
static GameObject* sHitObj;
static HealthGuard sHitGuard;
static GameObject* sMainMoveObj;
static ClimbSpeed sClimbSpeed;
static GameObject* sLadderMountObj;
static float sLadderMountScale;

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
static int (*origPlayerStateClimbOntoLadder)(GameObject*, PlayerState*, float);
static int (*origObjectObjAnimAdvanceMove)(void*, float, float, void*);
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

/* Ladder Speed. Both climb states start from the speed stored on the last tick
   and store a speed again before returning 0; the early exits return non-zero
   and store nothing. Moving computes a fresh speed (signed on ladders, negative
   going down), but idling on a ladder or wall and getting off a ladder store
   last tick's speed back unchanged. So the speed this hook stored is swapped
   back for the game's own value before the state runs, and what the state
   leaves is scaled once: a scaled speed is never scaled again, and a new mode
   applies from the next tick. An early exit keeps last tick's speed, so the
   swapped-back value is scaled again for that tick. */

static float ladder_speed_scale(void) {
  switch (gCheatOn[CHEAT_LADDER_SPEED]) {
    case LADDER_SPEED_2X:
      return 2.0f;
    case LADDER_SPEED_4X:
      return 4.0f;
    default:
      return 1.0f;
  }
}

static int run_climb_state(int (*original)(GameObject*, PlayerState*), GameObject* obj, PlayerState* state) {
  float scale = ladder_speed_scale();
  int restored = 0;
  int result;

  if (state != NULL && state == sClimbSpeed.state) {
    restored = state->moveSpeed == sClimbSpeed.scaled;
    if (restored) {
      state->moveSpeed = sClimbSpeed.game;
    }
    sClimbSpeed.state = NULL;
  }
  result = original(obj, state);
  if (scale != 1.0f && (result == 0 || restored) && state != NULL && obj == game.Obj_GetPlayerObject()) {
    sClimbSpeed.state = state;
    sClimbSpeed.game = state->moveSpeed;
    state->moveSpeed *= scale;
    sClimbSpeed.scaled = state->moveSpeed;
  }
  return result;
}

static int hookPlayerStateClimbWall(GameObject* obj, PlayerState* state) {
  return run_climb_state(origPlayerStateClimbWall, obj, state);
}

static int hookPlayerStateOnLadder(GameObject* obj, PlayerState* state) {
  return run_climb_state(origPlayerStateOnLadder, obj, state);
}

/* Getting onto a ladder is its own state, before playerStateOnLadder. Every
   tick it stores a fresh speed (0.014 from the bottom, 0.01 from the top),
   advances one animation layer with it through Object_ObjAnimAdvanceMove and
   returns 0, and the state machine then advances the other layer with the
   stored speed. Both layers get the multiplier: the first while the state
   runs, the second by scaling what it stored. It never reads a speed before
   storing its own, so this cannot compound. */
static int hookObjectObjAnimAdvanceMove(void* anim, float moveStepScale, float dt, void* events) {
  if (anim != NULL && anim == sLadderMountObj) {
    moveStepScale *= sLadderMountScale;
  }
  return origObjectObjAnimAdvanceMove(anim, moveStepScale, dt, events);
}

static int hookPlayerStateClimbOntoLadder(GameObject* obj, PlayerState* state, float dt) {
  float scale = ladder_speed_scale();
  int result;

  sLadderMountObj = scale != 1.0f && obj == game.Obj_GetPlayerObject() ? obj : NULL;
  sLadderMountScale = scale;
  result = origPlayerStateClimbOntoLadder(obj, state, dt);
  if (result == 0 && sLadderMountObj != NULL && state != NULL) {
    state->moveSpeed *= sLadderMountScale;
  }
  sLadderMountObj = NULL;
  return result;
}

/* Fast Movement. playerUpdate is the only caller of
   playerUpdateSurfaceResponse. After it returns, playerUpdate runs
   playerUpdateVelocityFromMotion and clamps the velocity, then calls the
   objMove that applies the player's velocity, with no other objMove in between.
   Arming the player here makes that main objMove the next one for the player,
   so it is the only move scaled. Ladders and climbable walls belong to Ladder
   Speed alone. */
static void arm_main_move(GameObject* obj) {
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

/* Shared by God Mode and Fast Movement. Sinking surfaces drain one health per
   tick inside this function. Fast Movement arms its marker last, once the
   original has returned and the health guard is released. */
static void hookPlayerUpdateSurfaceResponse(GameObject* obj, PlayerState* state, PlayerState* cfg, float dt) {
  HealthGuard guard = {NULL, 0};
  PlayerStatus* status = gCheatOn[CHEAT_GOD_MODE] ? playerStatusOf(obj) : NULL;

  if (status != NULL && status->health > 0) {
    guard_health(&guard, status, status->health);
  }
  origPlayerUpdateSurfaceResponse(obj, state, cfg, dt);
  release_health(&guard);
  arm_main_move(obj);
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
    {"objMove", (HookFn)hookObjMove, (void**)&origObjMove, NULL},
};

static Hook sLadderSpeedHooks[] = {
    {"playerStateClimbWall", (HookFn)hookPlayerStateClimbWall, (void**)&origPlayerStateClimbWall, NULL},
    {"playerStateOnLadder", (HookFn)hookPlayerStateOnLadder, (void**)&origPlayerStateOnLadder, NULL},
    {"playerStateClimbOntoLadder", (HookFn)hookPlayerStateClimbOntoLadder, (void**)&origPlayerStateClimbOntoLadder,
     NULL},
    {"Object_ObjAnimAdvanceMove", (HookFn)hookObjectObjAnimAdvanceMove, (void**)&origObjectObjAnimAdvanceMove, NULL},
};

/* Installed once for every group that sets needsSharedHooks. */
static Hook sSharedHooks[] = {
    {"playerUpdateSurfaceResponse", (HookFn)hookPlayerUpdateSurfaceResponse, (void**)&origPlayerUpdateSurfaceResponse,
     NULL},
};

#define COUNT_OF(array) ((int)(sizeof(array) / sizeof((array)[0])))

static CheatGroup sGroups[] = {
    {CHEAT_GOD_MODE, kGodModeSymbols, COUNT_OF(kGodModeSymbols), sGodModeHooks, COUNT_OF(sGodModeHooks), 1},
    {CHEAT_FAST_MOVEMENT, NULL, 0, sFastMovementHooks, COUNT_OF(sFastMovementHooks), 1},
    {CHEAT_LADDER_SPEED, NULL, 0, sLadderSpeedHooks, COUNT_OF(sLadderSpeedHooks), 0},
    {CHEAT_INFINITE_MAGIC, kInfiniteMagicSymbols, COUNT_OF(kInfiniteMagicSymbols), sInfiniteMagicHooks,
     COUNT_OF(sInfiniteMagicHooks), 0},
    {CHEAT_INFINITE_TRICKY_ENERGY, kTrickySymbols, COUNT_OF(kTrickySymbols), NULL, 0, 0},
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

/* The shared hooks go in first, and come out again when no group that needs
   them ended up available. */
int playerHooksInstall(FhMod* mod, const FhModHost* host) {
  int sharedOk;
  int sharedUsed = 0;
  int available = 0;
  int i;

  if (!resolve_symbols(mod, host, kCoreSymbols, COUNT_OF(kCoreSymbols), FH_LOG_ERROR)) {
    return 0;
  }
  sharedOk = install_hooks(mod, host, sSharedHooks, COUNT_OF(sSharedHooks), FH_LOG_WARN);
  for (i = 0; i < COUNT_OF(sGroups); i++) {
    CheatGroup* group = &sGroups[i];
    int ok = (!group->needsSharedHooks || sharedOk) &&
             resolve_symbols(mod, host, group->symbols, group->symbolCount, FH_LOG_WARN) &&
             install_hooks(mod, host, group->hooks, group->hookCount, FH_LOG_WARN);

    gCheatAvailable[group->cheat] = (unsigned char)ok;
    if (ok) {
      available++;
      sharedUsed |= group->needsSharedHooks;
    } else {
      modLog(FH_LOG_WARN, "%s unavailable: required game symbols or hooks are missing", gCheatNames[group->cheat]);
    }
  }
  if (!sharedUsed) {
    remove_hooks(mod, host, sSharedHooks, COUNT_OF(sSharedHooks));
  }
  return available > 0;
}

void playerHooksRemove(FhMod* mod, const FhModHost* host) {
  int i;

  for (i = COUNT_OF(sGroups) - 1; i >= 0; i--) {
    remove_hooks(mod, host, sGroups[i].hooks, sGroups[i].hookCount);
    gCheatAvailable[sGroups[i].cheat] = 0;
  }
  remove_hooks(mod, host, sSharedHooks, COUNT_OF(sSharedHooks));
  sMessageObj = NULL;
  sHitObj = NULL;
  sHitGuard.status = NULL;
  sMainMoveObj = NULL;
  sClimbSpeed.state = NULL;
  sLadderMountObj = NULL;
  memset(&game, 0, sizeof(game));
}
