#include "saw.h"

struct _TSaw {
  TTrap parent_instance;
};

G_DEFINE_TYPE(TSaw, t_saw, T_TYPE_TRAP)

static void t_saw_class_init(TSawClass *klass) {
//     SpriteClass *sprite_class = T_SPRITE_CLASS(klass);
//   sprite_class->sprite_pre_update = t_falling_platform_pre_update;
//   sprite_class->sprite_post_update = t_falling_platform_post_update;
//   sprite_class->sprite_render = t_falling_platform_render;
}

static void t_saw_init(TSaw *self) {}
