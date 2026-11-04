#ifndef PLAY_H
#define PLAY_H

#include "collision_handler.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum { IDLE, RUNNING, JUMPING_OR_FALLING } PlayerState;
typedef enum { SHOW_IDLE, SHOW_RUNNING, SHOW_JUMPING_OR_FALLING, SHOW_DOUBLE_JUMPING } ShowState;
#define PLAYER_SPRITES 0

typedef struct {
  float x;
  float y;
  bool facing_right;
  PlayerState state;
  CollisionOffset hitbox_offset;
  float velocity_y;
  bool grounded;
  bool double_jumping;
  bool jump_was_down;
} Player;

extern void init_player(Player *player, const char *tmx_object_name,
                        const char *spriteset_name);

extern void update_player(Player *player, uint32_t frame);

extern void render_player(Player *player, uint32_t frame);

#endif