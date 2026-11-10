#include "player.h"
#include "../../src/ObjectList.h"
#include "Tilengine.h"
#include "collision_handler.h"
#include "env_interaction.h"
#include "sprite.h"
#include <math.h>
#include <stdbool.h>

#define MOVE_SPEED 120.0f
#define SLIDE_SPEED 90.0f
// 数值越大，起跳和下落越快，高度保持不变。
#define JUMP_PACE 1.9f
#define JUMP_SPEED (160.0f * JUMP_PACE)
#define GRAVITY (300.0f * JUMP_PACE * JUMP_PACE)

typedef enum { IDLE, RUNNING, JUMPING_OR_FALLING, SLIDING } PlayerState;
typedef enum {
  SHOW_IDLE,
  SHOW_RUNNING,
  SHOW_JUMPING_OR_FALLING,
  SHOW_DOUBLE_JUMPING,
  SHOW_SLIDING
} ShowState;

struct _Player {
  Sprite parent_instance;
  bool facing_right;
  PlayerState state;
  float velocity_y;
  bool grounded;
  bool touching_wall;
  bool double_jumping;
  bool jump_was_down;
  ShowState show_state;
};

G_DEFINE_TYPE(Player, player, T_TYPE_SPRITE)

static TLN_Spriteset idle_set;
static TLN_Spriteset running_set;
static TLN_Spriteset jump_set;
static TLN_Spriteset double_jump_set;
static TLN_Spriteset sliding_set;

static TLN_Sequence idle_seq;
static TLN_Sequence running_seq;
static TLN_Sequence double_jump_seq;
static TLN_Sequence sliding_seq;

typedef struct {
  int direction; // -1 向左，0 不移动，1 向右
  bool jump_pressed;
} PlayerInput;

typedef struct {
  float dx, dy;
} PlayerMotion;

// 由glib自动调用
static void player_init(Player *self) {
  self->facing_right = true;
  self->state = IDLE;
  self->velocity_y = 0.0f;
  self->grounded = true;
  self->touching_wall = false;
  self->double_jumping = false;
  self->jump_was_down = false;
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

static PlayerMotion advance_status(Player *player, const PlayerInput *input,
                                   uint32_t elapsed) {
  float dt = elapsed / 1000.0f;
  PlayerMotion motion = {.dx = input->direction * MOVE_SPEED * dt};
  if (input->direction != 0)
    player->facing_right = input->direction > 0;

  if (player->grounded) {      // 角色处于地面
    if (input->jump_pressed) { // 地面上跳
      player->state = JUMPING_OR_FALLING;
      player->grounded = false;
      player->velocity_y = -JUMP_SPEED;
      motion.dy = player->velocity_y * dt + 0.5f * GRAVITY * dt * dt;
      player->velocity_y += GRAVITY * dt;
    } else { // 地面上水平移动
      player->state = motion.dx != 0 ? RUNNING : IDLE;
      player->velocity_y = 0;
    }
    player->double_jumping = false;
  } else {                                                // 角色处于空中
    if (input->jump_pressed && !player->double_jumping) { // 二段跳
      player->double_jumping = true;
      player->velocity_y = -JUMP_SPEED;
    }

    if (player->touching_wall && player->velocity_y > 0) { // 向下滑行中
      player->state = SLIDING;
      player->velocity_y = SLIDE_SPEED;
      motion.dy = SLIDE_SPEED * dt;
    } else {
      player->state = JUMPING_OR_FALLING;
      motion.dy = player->velocity_y * dt + 0.5f * GRAVITY * dt * dt;
      player->velocity_y += GRAVITY * dt;
    }
  }
  return motion;
}

static void player_pre_update(Sprite *sprite, uint32_t elapsed) {
  Player *player = T_PLAYER(sprite);
  SpriteData *data = get_sprite_data(sprite);
  PlayerInput input = read_player_input(player);
  // 通过输入和当前状态决定期望位移和切换到的状态
  PlayerMotion motion = advance_status(player, &input, elapsed);
  data->desired_dx = motion.dx;
  data->desired_dy = motion.dy;
}

static void player_post_update(Sprite *sprite, uint32_t elapsed) {
  Player *player = T_PLAYER(sprite);
  SpriteData *data = get_sprite_data(sprite);
  data->rect.x += data->resolved_dx;
  data->rect.y += data->resolved_dy;
  // 顶头后立即停止上升
  if (data->desired_dy < 0 && fabsf(data->resolved_dy - data->desired_dy) >
                                  0.0001f) // 浮点数有误差,不要精确判断
    player->velocity_y = 0;
  HitBox box = get_sprite_hitbox(sprite);
  player->grounded = is_player_grounded(player);
  player->touching_wall = is_touching_wall(box);
  if (player->grounded) {
    player->velocity_y = 0;
    player->double_jumping = false;
    player->state = data->desired_dx != 0 ? RUNNING : IDLE;
  } else if (player->touching_wall && player->velocity_y > 0) {
    player->state = SLIDING;
  } else {
    player->state = JUMPING_OR_FALLING;
  }
}

static void player_render(Sprite *sprite, uint32_t elapsed) {
  Player *player = T_PLAYER(sprite);
  SpriteData *data = get_sprite_data(sprite);
  TLN_SetSpriteWorldPosition(data->sprite_index, data->rect.x, data->rect.y);
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

  TLN_SetSpriteFlags(data->sprite_index, player->facing_right ? 0 : FLAG_FLIPX);
  if (player->show_state == next_show)
    return;
  TLN_SetSpriteSet(data->sprite_index, spriteset);
  if (sequence)
    TLN_SetSpriteAnimation(data->sprite_index, sequence, 0);
  else
    TLN_DisableSpriteAnimation(data->sprite_index);
  player->show_state = next_show;
}

static void player_class_init(PlayerClass *klass) {
  SpriteClass *sprite_class = T_SPRITE_CLASS(klass);

  sprite_class->sprite_pre_update = player_pre_update;
  sprite_class->sprite_post_update = player_post_update;
  sprite_class->sprite_render = player_render;
}

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

Player *player_new(TLN_Object *obj) {

  init_animation();
  Player *player = T_PLAYER(g_object_new(T_TYPE_PLAYER, NULL));

  spritelize(T_SPRITE(player), obj, idle_set, idle_seq);

  SpriteData *data = get_sprite_data(T_SPRITE(player));
  data->hitbox_offset =
      (CollisionOffset){.x_offset = 5, .y_offset = 4, .w = 22, .h = 28};
  player->grounded = is_player_grounded(player);
  player->touching_wall = is_touching_wall(get_sprite_hitbox(T_SPRITE(player)));
  player->state = player->grounded ? IDLE : JUMPING_OR_FALLING;
  return player;
}

float player_get_velocity_y(Player *self) {
  g_return_val_if_fail(T_IS_PLAYER(self), 0.0f);
  return self->velocity_y;
}