#include "platform_input.h"
#include "player_cheats.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Keys indexed by Cheat: 1 God Mode, 2 Fast Movement, 3 Ladder Speed, 4 Infinite Magic, 5 Infinite Tricky Energy.
   Each cheat accepts its number-row key or the matching numpad key. */
static const int kCheatKeys[CHEAT_COUNT] = {'1', '2', '3', '4', '5'};
static const int kCheatNumpadKeys[CHEAT_COUNT] = {VK_NUMPAD1, VK_NUMPAD2, VK_NUMPAD3, VK_NUMPAD4, VK_NUMPAD5};

int platformInputInitialize(FhMod* mod, const FhModHost* host) {
  (void)mod;
  (void)host;
  return 1;
}

void platformInputShutdown(void) {
}

int platformInputActive(void) {
  HWND window = GetForegroundWindow();
  DWORD processId = 0;

  if (window == NULL) return 0;
  GetWindowThreadProcessId(window, &processId);
  return processId == GetCurrentProcessId();
}

static int key_down(int key) {
  return (GetAsyncKeyState(key) & 0x8000) != 0;
}

/* Both keys of a cheat are one control: it is down while either key is down.
   Windows reports VK_NUMPAD1-5 only with Num Lock on. */
int platformCheatKeyDown(int cheat) {
  if (cheat < 0 || cheat >= CHEAT_COUNT) return 0;
  return key_down(kCheatKeys[cheat]) || key_down(kCheatNumpadKeys[cheat]);
}
