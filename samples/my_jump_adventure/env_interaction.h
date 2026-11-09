#ifndef ENV_INTERACTION_H
#define ENV_INTERACTION_H
#include "glib-object.h"
#include "player.h"
#include <stdbool.h>

extern bool is_player_grounded(Player *player);

extern bool is_touching_wall(HitBox box);

extern void init_env_interaction();

extern GPtrArray *get_traps_array();

extern HitBox handle_traps_collision(HitBox src, float dx, float dy);
#endif