#ifndef PLAYER_H
#define PLAYER_H

#include "sprite.h"
#include "../../src/ObjectList.h"

G_BEGIN_DECLS

#define T_TYPE_PLAYER (player_get_type())

G_DECLARE_FINAL_TYPE(Player, player, T, PLAYER, Sprite)

Player *player_new(TLN_Object *obj);

float player_get_velocity_y(Player *self);

void die(Player *self);
bool player_is_dead(Player *self);
/* Returns true once the one-shot disappearance animation has finished. */
bool update_player_death(Player *self);

G_END_DECLS

#endif