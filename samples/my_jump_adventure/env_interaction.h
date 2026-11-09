#ifndef ENV_INTERACTION_H
#define ENV_INTERACTION_H
#include "glib-object.h"
#include "player.h"
#include <stdbool.h>

extern bool is_player_grounded(Player *player);

extern bool is_touching_wall(HitBox box);

extern void init_env_interaction();

/* Move traps and carry supported players before update_player(). */
extern void update_env_interaction(Player *player, uint32_t elapsed);

extern GPtrArray *get_traps_array();

extern HitBox handle_traps_collision(HitBox src, float dx, float dy);
#endif
