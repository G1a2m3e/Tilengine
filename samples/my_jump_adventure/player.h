#ifndef PLAY_H
#define PLAY_H

#include "collision_handler.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum { IDLE, RUNNING, JUMPING_OR_FALLING } PlayerState;
#define PLAYER_SPRITES 0

typedef struct {
  float x;
  float y;
  bool facing_right;
  PlayerState state;
  HitBoxOffset hitbox_offset;
  float velocity_y;
  bool grounded;
} Player;

extern void init_player(Player *player, const char *tmx_object_name,
                        const char *spriteset_name);

extern void update_player(Player *player, uint32_t frame);

extern void render_player(Player *player, uint32_t frame);

#endif