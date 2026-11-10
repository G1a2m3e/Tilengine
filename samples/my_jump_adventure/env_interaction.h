#ifndef ENV_INTERACTION_H
#define ENV_INTERACTION_H
#include "player.h"
#include <stdbool.h>

extern bool is_player_grounded(void);

extern bool is_touching_wall(HitBox box);

extern void init_traps();

extern void init_player(const char *player_name);

extern Player *get_player();

extern GPtrArray *get_traps_array();

extern void update_env_interaction(uint32_t elapsed);

extern HitBox handle_traps_collision(HitBox src, float dx, float dy);
#endif
