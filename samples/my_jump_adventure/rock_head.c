#include "rock_head.h"

#include "utils.h"
#include <math.h>

struct _TRockhead {
  TTrap parent_instance;
  MotionPath path;
};

G_DEFINE_TYPE(TRockhead, t_rockhead, T_TYPE_TRAP)

static void rockhead_pre_update(Sprite *sprite, uint32_t elapsed) {
  TRockhead *self = T_ROCKHEAD(sprite);
  SpriteData *data = get_sprite_data(sprite);
}

static void rockheadpost_update(Sprite *sprite, uint32_t elapsed) {
  g_return_if_fail(T_ROCKHEAD(sprite));
  SpriteData *data = get_sprite_data(sprite);
}
static void render_rockhead(Sprite *sprite, uint32_t elapsed) {
  g_return_if_fail(T_ROCKHEAD(sprite));
  SpriteData *data = get_sprite_data(sprite);
  TLN_SetSpriteWorldPosition(data->sprite_index, data->rect.x, data->rect.y);
}
static void rockhead_ready(Sprite *sprite) {
  TRockhead *self = T_ROCKHEAD(sprite);
  SpriteData *data = get_sprite_data(sprite);
  free_object_path(&self->path);
}

static void T_ROCKHEAD_finalize(GObject *object) {
  free_object_path(&T_ROCKHEAD(object)->path);
  G_OBJECT_CLASS(t_rockhead_parent_class)->finalize(object);
}

static void t_rockhead_class_init(TRockheadClass *klass) {
  G_OBJECT_CLASS(klass)->finalize = T_ROCKHEAD_finalize;
  SpriteClass *sprite_class = T_SPRITE_CLASS(klass);
  sprite_class->sprite_ready = rockhead_ready;
  sprite_class->sprite_pre_update = rockhead_pre_update;
  sprite_class->sprite_post_update = rockheadpost_update;
  sprite_class->sprite_render = render_rockhead;
}

static void t_rockhead_init(TRockhead *self) { self->path = (MotionPath){0}; }
