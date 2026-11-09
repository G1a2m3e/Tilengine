#include "falling_platform.h"
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
static TLN_Spriteset off_set;

G_DEFINE_TYPE(TFallingPlatform, t_falling_platform, T_TYPE_TRAP)

void t_falling_platform_touch(TFallingPlatform *self) {
  g_return_if_fail(T_IS_FALLING_PLATFORM(self));
  if (!self->touched && t_trap_get_data(T_TRAP(self))->active) {
    self->touched = TRUE;
    self->delay_remaining = FALL_DELAY_MS;
  }
}

gboolean t_falling_platform_is_offscreen(TFallingPlatform *self,
                                        int viewport_bottom) {
  g_return_val_if_fail(T_IS_FALLING_PLATFORM(self), FALSE);
  return self->falling &&
         t_trap_get_data(T_TRAP(self))->hitbox.y >= viewport_bottom;
}

static void floating(TFallingPlatform *self, TTrapData *data,
                     uint32_t elapsed) {
  if (!self->initialized) {
    self->init_y = data->hitbox.y;
    self->initialized = TRUE;
  }
  float dt = elapsed / 1000.0f;
  if (data->hitbox.y <= self->init_y - 8) {
    self->direction = 1;
  }
  if (data->hitbox.y >= self->init_y + 8) {
    self->direction = -1;
  }
  data->hitbox.y = data->hitbox.y + (self->direction * MOVE_SPEED * dt);
}

static void t_falling_platform_update(TTrap *trap, Player *player,
                                      uint32_t elapsed) {
  TFallingPlatform *self = T_FALLING_PLATFORM(trap);
  TTrapData *data = t_trap_get_data(trap);
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
  data->hitbox.y += self->velocity_y * dt + 0.5f * FALL_GRAVITY * dt * dt;
  self->velocity_y += FALL_GRAVITY * dt;
}

static void t_falling_platform_render(TTrap *trap, Player *player,
                                      uint32_t elapse) {
  TFallingPlatform *self = T_FALLING_PLATFORM(trap);
  TTrapData *data = t_trap_get_data(trap);
  TLN_SetSpriteWorldPosition(data->sprite_index, data->hitbox.x,
                             data->hitbox.y);
}

static void t_falling_platform_class_init(TFallingPlatformClass *klass) {
  TTrapClass *trap_class = T_TRAP_CLASS(klass);

  trap_class->update = t_falling_platform_update;
  trap_class->render = t_falling_platform_render;
}

static void t_falling_platform_init(TFallingPlatform *self) {
  self->velocity_y = 0.0f;
  self->falling = false;
  self->init_y = 0;
  self->direction = -1;
}
