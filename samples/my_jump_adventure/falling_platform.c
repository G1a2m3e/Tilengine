#include "falling_platform.h"
#include <math.h>
#include <stdbool.h>

struct _TFallingPlatform {
  TTrap parent_instance;
  float velocity_y;
  gboolean falling;
  int direction;
  float init_y;
  gboolean initialized;
  gboolean touched;
  uint32_t delay_remaining;
};

static const float MOVE_SPEED = 7.0f;
static const float FALL_GRAVITY = 600.0f;
static const uint32_t FALL_DELAY_MS = 500;
static const float CONTACT_EPSILON = 0.001f;
static TLN_Spriteset off_set;

G_DEFINE_TYPE(TFallingPlatform, t_falling_platform, T_TYPE_TRAP)

void t_falling_platform_touch(TFallingPlatform *self) {
  g_return_if_fail(T_IS_FALLING_PLATFORM(self));
  if (!self->touched && get_sprite_data(T_SPRITE(self))->active) {
    self->touched = TRUE;
    self->delay_remaining = FALL_DELAY_MS;
  }
}

static gboolean is_standing_on_platform(HitBox box, HitBox platform) {
  return box.x < platform.x + platform.w && box.x + box.w > platform.x &&
         fabsf(box.y + box.h - platform.y) <= CONTACT_EPSILON;
}

gboolean t_falling_platform_is_supporting(TFallingPlatform *self, HitBox box) {
  g_return_val_if_fail(T_IS_FALLING_PLATFORM(self), FALSE);
  return get_sprite_data(T_SPRITE(self))->active &&
         is_standing_on_platform(box, get_sprite_hitbox(T_SPRITE(self)));
}

gboolean t_falling_platform_carry(TFallingPlatform *self, HitBox box, float *dx,
                                  float *dy) {
  g_return_val_if_fail(T_IS_FALLING_PLATFORM(self), FALSE);
  if (!get_sprite_data(T_SPRITE(self))->active)
    return FALSE;

  HitBox previous = t_trap_get_data(T_TRAP(self))->previous_hitbox;
  if (!is_standing_on_platform(box, previous))
    return FALSE;

  HitBox current = get_sprite_hitbox(T_SPRITE(self));
  float carry_dx = current.x - previous.x;
  float left = box.x + *dx + carry_dx;
  if (left >= current.x + current.w || left + box.w <= current.x)
    return FALSE;

  *dx += carry_dx;
  *dy += current.y - (box.y + box.h);
  t_falling_platform_touch(self);
  return TRUE;
}

gboolean t_falling_platform_check_collision(TFallingPlatform *self, HitBox src,
                                            float dx, float dy, float *hit_t,
                                            HitBox *result) {
  g_return_val_if_fail(T_IS_FALLING_PLATFORM(self), FALSE);
  if (!get_sprite_data(T_SPRITE(self))->active)
    return FALSE;

  HitBox platform = get_sprite_hitbox(T_SPRITE(self));
  HitBox previous = t_trap_get_data(T_TRAP(self))->previous_hitbox;
  float bottom = src.y + src.h;
  float relative_dy = dy - (platform.y - previous.y);
  if (relative_dy <= 0 || bottom > previous.y + CONTACT_EPSILON)
    return FALSE;

  float time = fmaxf(0, (previous.y - bottom) / relative_dy);
  float left = src.x + (dx - (platform.x - previous.x)) * time;
  if (time > 1.0f || left >= previous.x + previous.w ||
      left + src.w <= previous.x)
    return FALSE;

  *hit_t = time;
  *result = src;
  result->x += dx;
  result->y = platform.y - src.h;
  return TRUE;
}

gboolean t_falling_platform_is_offscreen(TFallingPlatform *self,
                                         int viewport_bottom) {
  g_return_val_if_fail(T_IS_FALLING_PLATFORM(self), FALSE);
  return self->falling &&
         get_sprite_data(T_SPRITE(self))->rect.y >= viewport_bottom;
}

static void floating(TFallingPlatform *self, SpriteData *data,
                     uint32_t elapsed) {
  if (!self->initialized) {
    self->init_y = data->rect.y;
    self->initialized = TRUE;
  }
  float dt = elapsed / 1000.0f;
  if (data->rect.y <= self->init_y - 8) {
    self->direction = 1;
  }
  if (data->rect.y >= self->init_y + 8) {
    self->direction = -1;
  }
  data->rect.y = data->rect.y + (self->direction * MOVE_SPEED * dt);
}

static void t_falling_platform_pre_update(Sprite *sprite, uint32_t elapsed) {
  TFallingPlatform *self = T_FALLING_PLATFORM(sprite);
  SpriteData *data = get_sprite_data(sprite);
  t_trap_get_data(T_TRAP(self))->previous_hitbox = get_sprite_hitbox(sprite);
  if (!self->touched) {
    floating(self, data, elapsed);
    return;
  }

  if (!self->falling) {
    uint32_t waiting = MIN(elapsed, self->delay_remaining);
    floating(self, data, waiting);
    self->delay_remaining -= waiting;
    elapsed -= waiting;
    if (self->delay_remaining > 0)
      return;

    self->falling = TRUE;
    TLN_DisableSpriteAnimation(data->sprite_index);
    if (!off_set)
      off_set = TLN_LoadSpriteset("Traps/Falling Platforms/Off.png");
    if (off_set)
      TLN_SetSpriteSet(data->sprite_index, off_set);
  }

  float dt = elapsed / 1000.0f;
  data->rect.y += self->velocity_y * dt + 0.5f * FALL_GRAVITY * dt * dt;
  self->velocity_y += FALL_GRAVITY * dt;
}

static void t_falling_platform_post_update(Sprite *sprite, uint32_t elapsed) {}

static void t_falling_platform_render(Sprite *sprite, uint32_t elapsed) {
  SpriteData *data = get_sprite_data(sprite);
  TLN_SetSpriteWorldPosition(data->sprite_index, data->rect.x, data->rect.y);
}

static void t_falling_platform_class_init(TFallingPlatformClass *klass) {
  SpriteClass *sprite_class = T_SPRITE_CLASS(klass);
  sprite_class->sprite_pre_update = t_falling_platform_pre_update;
  sprite_class->sprite_post_update = t_falling_platform_post_update;
  sprite_class->sprite_render = t_falling_platform_render;
}

static void t_falling_platform_init(TFallingPlatform *self) {
  self->velocity_y = 0.0f;
  self->falling = false;
  self->init_y = 0;
  self->direction = -1;
}
