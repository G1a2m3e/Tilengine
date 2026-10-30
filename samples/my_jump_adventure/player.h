#ifndef PLAY_H
#define PLAY_H

#include <stdbool.h>

typedef enum { IDLE, RUNNING, JUMPING } PlayerState;
#define PLAYER_SPRITES 0

typedef struct {
  float x;
  float y;
  bool facing_right;
  PlayerState state;
} Player;

void init_player(Player *player, const char *name, const char *spriteset_name);
void player_move_to_right(Player *player);
void player_move_to_left(Player *player);
void player_idle(Player *player);

#endif