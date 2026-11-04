#include "player.h"
#include "Tilengine.h"
#include "collision_handler.h"
#include "utils.h"

static TLN_Spriteset idle_set;
static TLN_Spriteset running_set;
static TLN_Sequence idle_seq;
static TLN_Sequence running_seq;

void init_player(Player *player, const char *tmx_object_name,
                 const char *spriteset_name) {
  TLN_ObjectInfo obj = getObjectInfoByName(tmx_object_name);
  idle_set = TLN_LoadSpriteset(spriteset_name);
  running_set =
      TLN_LoadSpriteset("Main Characters/Virtual Guy/Run (32x32).png");
  idle_seq = TLN_CreateSpriteSequence("idle", idle_set, "idle", 3);
  running_seq = TLN_CreateSpriteSequence("running", running_set, "running", 3);
  TLN_SetSpriteSet(PLAYER_SPRITES, idle_set);
  player->x = obj.x;
  player->y = obj.y;
  TLN_SetSpritePosition(PLAYER_SPRITES, player->x, player->y);
  TLN_SetSpriteAnimation(PLAYER_SPRITES, idle_seq, 0);
  TLN_DisableLayer(0);
  player->facing_right = true;
  player->state = IDLE;
  player->hitbox = load_object_hitbox("my_jump_adventure.tmx", 72);
}

static int move_to_right(Player *player) {
  player->facing_right = true;
  player->state = RUNNING;
  TLN_SetSpriteFlags(PLAYER_SPRITES, 0);
  return 2;
}
static int move_to_left(Player *player) {
  player->facing_right = false;
  player->state = RUNNING;
  TLN_SetSpriteFlags(PLAYER_SPRITES, FLAG_FLIPX);
  return -2;
}
static void show_idle(Player *player) {
  TLN_SetSpriteSet(PLAYER_SPRITES, idle_set);
  TLN_SetSpriteAnimation(PLAYER_SPRITES, idle_seq, 0);
}

static void show_running(Player *player) {
  TLN_SetSpriteSet(PLAYER_SPRITES, running_set);
  TLN_SetSpriteAnimation(PLAYER_SPRITES, running_seq, 0);
}

void update_player(Player *player, uint32_t frame) {
  PlayerState last_state = player->state;
  float last_x = player->x;
  float last_y = player->y;

  bool right = TLN_GetInput(INPUT_RIGHT);
  bool left = TLN_GetInput(INPUT_LEFT);
  float dx = 0;
  float dy = 0;
  if (right) {
    dx = move_to_right(player);
  }
  if (left) {
    dx = move_to_left(player);
  }
  if (dx != 0 || dy != 0) {
    HitBox rect = {
        .x = player->x + player->hitbox.x,
        .y = player->y + player->hitbox.y,
        .w = player->hitbox.w,
        .h = player->hitbox.h,
        .push_weight = 1,
    };
    handle_maps_collision(rect, &dx, &dy);
  }
  player->x += dx;
  player->y += dy;
  if (last_x == player->x && last_y == player->y)
    player->state = IDLE;

  if (last_state != player->state) {
    if (player->state == RUNNING)
      show_running(player);
    else if (player->state == IDLE)
      show_idle(player);
  }
}

void render_player(Player *player, uint32_t frame) {
  TLN_SetSpritePosition(0, player->x, player->y);
}
