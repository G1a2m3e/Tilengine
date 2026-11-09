#include "falling_platform.h"
#include <stdbool.h>

struct _TFallingPlatform {
  TTrap parent_instance;
  float velocity_y;
  gboolean falling;
  int direction;
  float init_y;
  gboolean initialized;
};

static const float MOVE_SPEED = 7.0f;

G_DEFINE_TYPE(TFallingPlatform, t_falling_platform, T_TYPE_TRAP)

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
  if (!self->falling) {
    floating(self, data, elapsed);
  } else {
  }
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
