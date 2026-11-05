#include "player.h"
#include "Tilengine.h"
#include "collision_handler.h"
#include "utils.h"
#include <stdbool.h>

static TLN_Spriteset idle_set;
static TLN_Spriteset running_set;
static TLN_Spriteset jump_set;
static TLN_Spriteset double_jump_set;
static TLN_Spriteset sliding_set;

static TLN_Sequence idle_seq;
static TLN_Sequence running_seq;
static TLN_Sequence double_jump_seq;
static TLN_Sequence sliding_seq;

static ShowState show_state;
static bool showing_double_jump = false;
#define MOVE_SPEED 120.0f
#define SLIDE_SPEED 90.0f
// 数值越大，起跳和下落越快，高度保持不变。
#define JUMP_PACE 1.9f
#define JUMP_SPEED (160.0f * JUMP_PACE)
#define GRAVITY (300.0f * JUMP_PACE * JUMP_PACE)

static void init_animation() {
  idle_set = TLN_LoadSpriteset("Main Characters/Virtual Guy/Idle (32x32).png");
  running_set =
      TLN_LoadSpriteset("Main Characters/Virtual Guy/Run (32x32).png");
  jump_set = TLN_LoadSpriteset("Main Characters/Virtual Guy/Jump (32x32).png");
  double_jump_set =
      TLN_LoadSpriteset("Main Characters/Virtual Guy/Double Jump (32x32).png");
  sliding_set =
      TLN_LoadSpriteset("Main Characters/Virtual Guy/Wall Jump (32x32).png");

  idle_seq = TLN_CreateSpriteSequence("idle", idle_set, "idle", 3);
  running_seq = TLN_CreateSpriteSequence("running", running_set, "running", 3);
  double_jump_seq = TLN_CreateSpriteSequence("double_jump", double_jump_set,
                                             "double_jump", 3);
  sliding_seq = TLN_CreateSpriteSequence("sliding", sliding_set, "sliding", 3);
}

void init_player(Player *player, const char *tmx_object_name) {
  TLN_ObjectInfo obj = getObjectInfoByName(tmx_object_name);

  init_animation();
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
  player->double_jumping = false;
  player->jump_was_down = false;
  player->hitbox_offset = load_object_hitbox("my_jump_adventure.tmx", 72);
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

static float sliding(Player *player, uint32_t elapsed) {
  return SLIDE_SPEED * (elapsed / 1000.0f);
}

static void show_idle(Player *player) {
  if (show_state == SHOW_IDLE) {
    return;
  }
  TLN_SetSpriteSet(PLAYER_SPRITES, idle_set);
  TLN_SetSpriteAnimation(PLAYER_SPRITES, idle_seq, 0);
  show_state = SHOW_IDLE;
}

static void show_running(Player *player) {
  if (show_state == SHOW_RUNNING) {
    return;
  }
  TLN_SetSpriteSet(PLAYER_SPRITES, running_set);
  TLN_SetSpriteAnimation(PLAYER_SPRITES, running_seq, 0);
  show_state = SHOW_RUNNING;
}

static void show_double_jumping(Player *player) {
  if (show_state == SHOW_DOUBLE_JUMPING) {
    return;
  }
  TLN_SetSpriteSet(PLAYER_SPRITES, double_jump_set);
  TLN_SetSpriteAnimation(PLAYER_SPRITES, double_jump_seq, 0);
  show_state = SHOW_DOUBLE_JUMPING;
}
static void show_jumping(Player *player) {
  if (show_state == SHOW_JUMPING_OR_FALLING) {
    return;
  }
  TLN_DisableSpriteAnimation(PLAYER_SPRITES);
  TLN_SetSpriteSet(PLAYER_SPRITES, jump_set);
  show_state = SHOW_JUMPING_OR_FALLING;
}

static void show_sliding(Player *player) {
  if (show_state == SHOW_SLIDING) {
    return;
  }
  TLN_SetSpriteSet(PLAYER_SPRITES, sliding_set);
  TLN_SetSpriteAnimation(PLAYER_SPRITES, sliding_seq, 0);
  show_state = SHOW_SLIDING;
}

static HitBox get_player_hitbox(Player *player) {
  HitBox box = {
      .x = player->x + player->hitbox_offset.x_offset,
      .y = player->y + player->hitbox_offset.y_offset,
      .w = player->hitbox_offset.w,
      .h = player->hitbox_offset.h,
  };
  return box;
}

static bool is_player_grounded(Player *player) {
  HitBox box = get_player_hitbox(player);
  return is_grounded(box);
}

static bool is_player_touching_wall(Player *player) {
  HitBox box = get_player_hitbox(player);
  return is_touching_wall(box);
}

void update_player(Player *player, uint32_t elapsed) {
  bool last_grounded = player->grounded;
  float last_x = player->x;
  float last_y = player->y;

  bool right = TLN_GetInput(INPUT_RIGHT);
  bool left = TLN_GetInput(INPUT_LEFT);
  bool up = TLN_GetInput(INPUT_B) || TLN_GetInput(INPUT_UP);
  bool jump_pressed = up && !player->jump_was_down;
  player->jump_was_down = up;
  float dx = 0;
  float dy = 0;

  player->grounded = is_player_grounded(player);
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

  if (jump_pressed) {
    if (player->grounded) {
      player->state = JUMPING_OR_FALLING;
      player->velocity_y = -JUMP_SPEED;
      player->grounded = false;
      player->double_jumping = false;
    } else if (player->state == JUMPING_OR_FALLING && !player->double_jumping) {
      player->velocity_y = -JUMP_SPEED;
      player->double_jumping = true;
    }
  }
  if (player->state == JUMPING_OR_FALLING) {
    if (is_player_touching_wall(player) && player->velocity_y > 0) {
      player->state = SLIDING;
    } else {
      dy = jump_or_falling(player, elapsed);
    }
  }
  if (player->state == SLIDING) {
    if (is_player_touching_wall(player)) {
      dy = sliding(player, elapsed);
    } else {
      player->state = JUMPING_OR_FALLING;
    }
  }
  if (dx != 0 || dy != 0) {
    HitBox box = get_player_hitbox(player);
    HitBox result = handle_maps_collision(box, dx, dy);
    if (dy < 0 && result.y != box.y + dy)
      player->velocity_y = 0; // 碰到天花板时及时下落
    player->x = result.x - player->hitbox_offset.x_offset;
    player->y = result.y - player->hitbox_offset.y_offset;
  }
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
    if (player->double_jumping && player->velocity_y < 0)
      show_double_jumping(player);
    else {
      show_jumping(player);
    }
  } else if (player->state == SLIDING) {
    show_sliding(player);
  }
}
