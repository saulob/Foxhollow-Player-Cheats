#ifndef PLATFORM_INPUT_H_
#define PLATFORM_INPUT_H_

#include "foxhollow_mod_api.h"

int platformInputInitialize(FhMod* mod, const FhModHost* host);
void platformInputShutdown(void);
int platformInputActive(void);
/* cheat is a Cheat index: 1 God Mode, 2 Fast Movement, 3 Ladder Speed, 4 Infinite Magic, 5 Infinite Tricky Energy. */
int platformCheatKeyDown(int cheat);

#endif
