#include "saw.h"

struct _TSaw {
  TTrap parent_instance;
};

G_DEFINE_TYPE(TSaw, t_saw, T_TYPE_TRAP)

static void saw_pre_update(Sprite *sprite, uint32_t elapsed) {
  g_return_if_fail(T_IS_SAW(sprite));
  SpriteData *data = get_sprite_data(sprite);
  data->desired_dx = 0;
  data->desired_dy = 0;
}

static void saw_post_update(Sprite *sprite, uint32_t elapsed) {
  g_return_if_fail(T_IS_SAW(sprite));
  SpriteData *data = get_sprite_data(sprite);
  data->resolved_dx = data->desired_dx;
  data->resolved_dy = data->desired_dy;
}
static void render_saw(Sprite *sprite, uint32_t elapsed) {
  g_return_if_fail(T_IS_SAW(sprite));
  SpriteData *data = get_sprite_data(sprite);
  TLN_SetSpriteWorldPosition(data->sprite_index, data->rect.x, data->rect.y);
}
static void t_saw_class_init(TSawClass *klass) {
  SpriteClass *sprite_class = T_SPRITE_CLASS(klass);
  sprite_class->sprite_pre_update = saw_pre_update;
  sprite_class->sprite_post_update = saw_post_update;
  sprite_class->sprite_render = render_saw;
}

static void t_saw_init(TSaw *self) {}
