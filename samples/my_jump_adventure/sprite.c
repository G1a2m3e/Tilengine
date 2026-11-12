#include "sprite.h"
#include "Tilengine.h"
#include "utils.h"

typedef struct {
  SpriteData data;
} SpritePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(Sprite, sprite, G_TYPE_OBJECT)

static void sprite_class_init(SpriteClass *klass) {
  klass->sprite_ready = NULL;
  klass->sprite_pre_update = NULL;
  klass->sprite_post_update = NULL;
  klass->sprite_render = NULL;
}

static void sprite_init(Sprite *self) {
  SpritePrivate *priv = sprite_get_instance_private(self);

  priv->data.sprite_index = -1;
  priv->data.rect = (DrawRect){0};
  priv->data.hitbox_offset = (CollisionOffset){0};
  priv->data.active = TRUE;
  priv->data.push_weight = 1;
  priv->data.desired_dx = 0;
  priv->data.desired_dy = 0;
  priv->data.resolved_dx = 0;
  priv->data.resolved_dy = 0;
}

void sprite_pre_update(Sprite *self, uint32_t elapsed) {
  g_return_if_fail(T_IS_SPRITE(self));
  if (!get_sprite_data(self)->active)
    return;

  SpriteClass *klass = T_SPRITE_GET_CLASS(self);

  if (klass->sprite_pre_update)
    klass->sprite_pre_update(self, elapsed);
}

void sprite_post_update(Sprite *self, uint32_t elapsed) {
  g_return_if_fail(T_IS_SPRITE(self));
  if (!get_sprite_data(self)->active)
    return;

  SpriteClass *klass = T_SPRITE_GET_CLASS(self);

  if (klass->sprite_post_update)
    klass->sprite_post_update(self, elapsed);
}

void sprite_render(Sprite *self, uint32_t elapsed) {
  g_return_if_fail(T_IS_SPRITE(self));
  if (!get_sprite_data(self)->active)
    return;

  SpriteClass *klass = T_SPRITE_GET_CLASS(self);

  if (klass->sprite_render)
    klass->sprite_render(self, elapsed);
}

HitBox get_sprite_hitbox(Sprite *self) {
  SpritePrivate *priv = sprite_get_instance_private(self);
  return (HitBox){
      .x = priv->data.rect.x + priv->data.hitbox_offset.x_offset,
      .y = priv->data.rect.y + priv->data.hitbox_offset.y_offset,
      .w = priv->data.hitbox_offset.w,
      .h = priv->data.hitbox_offset.h,
  };
}

SpriteData *get_sprite_data(Sprite *self) {
  SpritePrivate *priv = sprite_get_instance_private(self);
  return &priv->data;
}

void spritelize(Sprite *self, TLN_Object *object, TLN_Spriteset sprite_set,
                TLN_Sequence seq) {
  int sprite_index = TLN_GetAvailableSprite();
  TLN_SetSpriteSet(sprite_index, sprite_set);
  SpritePrivate *priv = sprite_get_instance_private(self);
  priv->data.sprite_index = sprite_index;
  priv->data.object_id = object->id;
  priv->data.active = TRUE;
  priv->data.rect.x = object->x;
  priv->data.rect.y = object->y;
  priv->data.rect.w = object->width;
  priv->data.rect.h = object->height;
  TLN_SetSpriteWorldPosition(sprite_index, object->x, object->y);
  TLN_SetSpriteAnimation(sprite_index, seq, 0);
  TLN_SetSpriteFlags(sprite_index, object->flags);
  bind_sprite_to_object(sprite_index, object);
  object->visible = false;
  SpriteClass *klass = T_SPRITE_GET_CLASS(self);
  if (klass->sprite_ready)
    klass->sprite_ready(self);
}

bool deactive_sprite(Sprite *self) {
  g_return_val_if_fail(T_IS_SPRITE(self), false);

  SpriteData *data = get_sprite_data(self);
  if (data->sprite_index >= 0) {
    TLN_DisableSpriteAnimation(data->sprite_index);
    if (!TLN_DisableSprite(data->sprite_index))
      return false;
    data->sprite_index = -1;
  }
  data->active = FALSE;
  data->desired_dx = data->desired_dy = 0;
  data->resolved_dx = data->resolved_dy = 0;
  return true;
}
