#include "player.h"
#include "../../src/ObjectList.h"
#include "Tilengine.h"
#include "collision_handler.h"
#include "env_interaction.h"
#include "game.h"
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
#define MOVE_SPEED 120.0f
#define SLIDE_SPEED 90.0f
// 数值越大，起跳和下落越快，高度保持不变。
#define JUMP_PACE 1.9f
#define JUMP_SPEED (160.0f * JUMP_PACE)
#define GRAVITY (300.0f * JUMP_PACE * JUMP_PACE)

typedef struct {
  int direction; // -1 向左，0 不移动，1 向右
  bool jump_pressed;
} PlayerInput;

typedef struct {
  float dx, dy;
} PlayerMotion;

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

void init_player(int current_level, Player *player,
                 const char *tmx_object_name) {
  TLN_ObjectInfo obj = getObjectInfoByName(tmx_object_name);

  init_animation();
  TLN_SetSpriteSet(PLAYER_SPRITES, idle_set);
  player->x = obj.x;
  player->y = obj.y;
  TLN_SetSpriteWorldPosition(PLAYER_SPRITES, player->x, player->y);
  TLN_SetSpriteAnimation(PLAYER_SPRITES, idle_seq, 0);
  show_state = SHOW_IDLE;
  int player_layer = get_object_layer(tmx_object_name);
  if (player_layer >= 0) {
    TLN_ObjectList objects = TLN_GetLayerObjects(player_layer);
    for (TLN_Object *item = objects->list; item; item = item->next) {
      if (item->id == obj.id) {
        item->visible = false;
        break;
      }
    }
  }
  player->facing_right = true;
  player->state = IDLE;
  player->velocity_y = 0.0f;
  player->grounded = true;
  player->touching_wall = false;
  player->double_jumping = false;
  player->jump_was_down = false;
  player->hitbox_offset =
      load_object_hitbox(get_level_file_name(current_level), tmx_object_name);
}

static HitBox get_player_hitbox(const Player *player) {
  return (HitBox){
      .x = player->x + player->hitbox_offset.x_offset,
      .y = player->y + player->hitbox_offset.y_offset,
      .w = player->hitbox_offset.w,
      .h = player->hitbox_offset.h,
  };
}

static PlayerInput read_player_input(Player *player) {
  bool left = TLN_GetInput(INPUT_LEFT);
  bool right = TLN_GetInput(INPUT_RIGHT);
  bool jump_down = TLN_GetInput(INPUT_B) || TLN_GetInput(INPUT_UP);
  PlayerInput input = {
      .direction = left    ? -1
                   : right ? 1
                           : 0,
      .jump_pressed = jump_down && !player->jump_was_down,
  };
  player->jump_was_down = jump_down;
  return input;
}

static void update_player_contacts(Player *player) {
  HitBox box = get_player_hitbox(player);
  player->grounded = is_player_grounded(player);
  player->touching_wall = is_touching_wall(box);
}

static void handle_player_actions(Player *player, const PlayerInput *input) {
  if (player->grounded) {
    player->velocity_y = 0;
    player->double_jumping = false;
  }
  if (!input->jump_pressed || (!player->grounded && player->double_jumping))
    return;

  player->double_jumping = !player->grounded;
  player->grounded = false;
  player->velocity_y = -JUMP_SPEED;
}

static void update_player_state(Player *player, const PlayerMotion *motion) {
  if (player->grounded) {
    player->state = motion->dx != 0 ? RUNNING : IDLE;
    player->velocity_y = 0;
    player->double_jumping = false;
  } else if (player->touching_wall && player->velocity_y > 0) {
    player->state = SLIDING;
  } else {
    player->state = JUMPING_OR_FALLING;
  }
}

static PlayerMotion calculate_player_motion(Player *player,
                                            const PlayerInput *input,
                                            uint32_t elapsed) {
  float dt = elapsed / 1000.0f;
  PlayerMotion motion = {.dx = input->direction * MOVE_SPEED * dt};
  if (input->direction != 0)
    player->facing_right = input->direction > 0;

  if (player->grounded)
    return motion;

  if (player->touching_wall && player->velocity_y > 0) {
    motion.dy = SLIDE_SPEED * dt;
  } else {
    motion.dy = player->velocity_y * dt + 0.5f * GRAVITY * dt * dt;
    player->velocity_y += GRAVITY * dt;
  }
  return motion;
}

static void apply_player_motion(Player *player, PlayerMotion *motion) {
  if (motion->dx == 0 && motion->dy == 0)
    return;

  HitBox box = get_player_hitbox(player);
  HitBox result = handle_maps_collision(box, motion->dx, motion->dy);
  result = handle_traps_collision(box, result.x - box.x, result.y - box.y);
  if (motion->dy < 0 && result.y != box.y + motion->dy)
    player->velocity_y = 0;

  player->x = result.x - player->hitbox_offset.x_offset;
  player->y = result.y - player->hitbox_offset.y_offset;
  motion->dx = result.x - box.x;
  motion->dy = result.y - box.y;
  update_player_contacts(player);
}

static void update_player_animation(Player *player) {
  TLN_Spriteset spriteset = idle_set;
  TLN_Sequence sequence = idle_seq;
  ShowState next_show = SHOW_IDLE;

  switch (player->state) {
  case IDLE:
    break;
  case RUNNING:
    spriteset = running_set;
    sequence = running_seq;
    next_show = SHOW_RUNNING;
    break;
  case JUMPING_OR_FALLING:
    if (player->double_jumping && player->velocity_y < 0) {
      spriteset = double_jump_set;
      sequence = double_jump_seq;
      next_show = SHOW_DOUBLE_JUMPING;
    } else {
      spriteset = jump_set;
      sequence = NULL;
      next_show = SHOW_JUMPING_OR_FALLING;
    }
    break;
  case SLIDING:
    spriteset = sliding_set;
    sequence = sliding_seq;
    next_show = SHOW_SLIDING;
    break;
  }

  TLN_SetSpriteFlags(PLAYER_SPRITES, player->facing_right ? 0 : FLAG_FLIPX);
  if (show_state == next_show)
    return;
  TLN_SetSpriteSet(PLAYER_SPRITES, spriteset);
  if (sequence)
    TLN_SetSpriteAnimation(PLAYER_SPRITES, sequence, 0);
  else
    TLN_DisableSpriteAnimation(PLAYER_SPRITES);
  show_state = next_show;
}

void update_player(Player *player, uint32_t elapsed) {
  PlayerInput input = read_player_input(player);
  update_player_contacts(player);
  handle_player_actions(player, &input);
  PlayerMotion motion = calculate_player_motion(player, &input, elapsed);
  apply_player_motion(player, &motion);
  update_player_state(player, &motion);
}

void render_player(Player *player, uint32_t frame) {
  update_player_animation(player);
  TLN_SetSpriteWorldPosition(PLAYER_SPRITES, player->x, player->y);
}
