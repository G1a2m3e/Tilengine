#include "player.h"
#include "Tilengine.h"
#include "collision_handler.h"
#include "utils.h"
#include <stdbool.h>

static TLN_Spriteset idle_set;
static TLN_Spriteset running_set;
static TLN_Spriteset jump_set;
static TLN_Sequence idle_seq;
static TLN_Sequence running_seq;
static PlayerState show_state;
#define MOVE_SPEED 120.0f
#define JUMP_SPEED 200.0f
#define GRAVITY 300.0f

void init_player(Player *player, const char *tmx_object_name,
                 const char *spriteset_name) {
  TLN_ObjectInfo obj = getObjectInfoByName(tmx_object_name);
  idle_set = TLN_LoadSpriteset(spriteset_name);
  running_set =
      TLN_LoadSpriteset("Main Characters/Virtual Guy/Run (32x32).png");
  jump_set = TLN_LoadSpriteset("Main Characters/Virtual Guy/Jump (32x32).png");
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
  player->velocity_y = 0.0f;
  player->grounded = true;
  player->hitbox = load_object_hitbox("my_jump_adventure.tmx", 72);
}

static float move_to_right(Player *player, uint32_t elapsed) {
  player->facing_right = true;
  TLN_SetSpriteFlags(PLAYER_SPRITES, 0);
  return MOVE_SPEED * (elapsed / 1000.0f);
}
static float move_to_left(Player *player, uint32_t elapsed) {
  player->facing_right = false;
  TLN_SetSpriteFlags(PLAYER_SPRITES, FLAG_FLIPX);
  return -MOVE_SPEED * (elapsed / 1000.0f);
}

static float jump_or_falling(Player *player, uint32_t elapsed) {
  float dt = elapsed / 1000.0f;
  float dy = player->velocity_y * dt + 0.5f * GRAVITY * dt * dt;
  player->velocity_y += GRAVITY * dt;
  return dy;
}

static void show_idle(Player *player) {
  if (show_state == IDLE) {
    return;
  }
  TLN_SetSpriteSet(PLAYER_SPRITES, idle_set);
  TLN_SetSpriteAnimation(PLAYER_SPRITES, idle_seq, 0);
  show_state = IDLE;
}

static void show_running(Player *player) {
  if (show_state == RUNNING) {
    return;
  }
  TLN_SetSpriteSet(PLAYER_SPRITES, running_set);
  TLN_SetSpriteAnimation(PLAYER_SPRITES, running_seq, 0);
  show_state = RUNNING;
}

static void show_jumping(Player *player) {
  if (show_state == JUMPING_OR_FALLING) {
    return;
  }
  TLN_DisableSpriteAnimation(PLAYER_SPRITES);
  TLN_SetSpriteSet(PLAYER_SPRITES, jump_set);
  show_state = JUMPING_OR_FALLING;
}

bool is_grounded(Player *player) {
  float dx = 0;
  float dy = 2;

  HitBox rect = {
      .x = player->x + player->hitbox.x,
      .y = player->y + player->hitbox.y,
      .w = player->hitbox.w,
      .h = player->hitbox.h,
      .push_weight = 1,
  };
  handle_maps_collision(rect, &dx, &dy);
  return dy == 0;
}

void update_player(Player *player, uint32_t elapsed) {
  PlayerState last_state = player->state;
  bool last_grounded = player->grounded;
  float last_x = player->x;
  float last_y = player->y;

  bool right = TLN_GetInput(INPUT_RIGHT);
  bool left = TLN_GetInput(INPUT_LEFT);
  bool up = TLN_GetInput(INPUT_UP);
  float dx = 0;
  float dy = 0;

  player->grounded = is_grounded(player);
  if (player->grounded) {
    if (!last_grounded) {
      player->velocity_y = 0;
      player->state = IDLE;
    }
  } else {
    if (last_grounded) {
      player->state = JUMPING_OR_FALLING;
      player->velocity_y = 0;
    }
  }
  if (right) {
    dx = move_to_right(player, elapsed);
  }
  if (left) {
    dx = move_to_left(player, elapsed);
  }

  if (up && player->grounded) {
    player->state = JUMPING_OR_FALLING;
    player->velocity_y = -JUMP_SPEED;
    player->grounded = false;
  }
  if (player->state == JUMPING_OR_FALLING) {
    dy = jump_or_falling(player, elapsed);
  }
  if (dx != 0 || dy != 0) {
    HitBox rect = {
        .x = player->x + player->hitbox.x,
        .y = player->y + player->hitbox.y,
        .w = player->hitbox.w,
        .h = player->hitbox.h,
        .push_weight = 1,
    };
    float target_dy = dy;
    handle_maps_collision(rect, &dx, &dy);
    if (target_dy < 0 && dy == 0)
      player->velocity_y = 0; // 碰到天花板时及时下落
  }
  player->x += dx;
  player->y += dy;
  if (player->grounded) {
    if (player->x == last_x && player->y == last_y) {
      player->state = IDLE;
    } else {
      player->state = RUNNING;
    }
  }
}

void render_player(Player *player, uint32_t frame) {
  TLN_SetSpritePosition(0, player->x, player->y);
  if (player->state == RUNNING)
    show_running(player);
  else if (player->state == IDLE)
    show_idle(player);
  else if (player->state == JUMPING_OR_FALLING) {
    show_jumping(player);
  }
}
