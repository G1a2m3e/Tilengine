#ifndef COMPONENT_H
#define COMPONENT_H

#include "../../src/ObjectList.h"
#include "collision_handler.h"
#include <glib-object.h>
#include <stdint.h>

G_BEGIN_DECLS

#define T_TYPE_SPRITE (sprite_get_type())

G_DECLARE_DERIVABLE_TYPE(Sprite, sprite, T, SPRITE, GObject)

struct _SpriteClass {
  GObjectClass parent_class;
  // 对象 ID、位置及图集绑定完成后调用。
  void (*sprite_ready)(Sprite *self);
  // 读取输入、决定移动意图、处理跳跃请求
  void (*sprite_pre_update)(Sprite *self, uint32_t elapsed);
  // 解决碰撞，确定实际位移和接触信息
  void (*sprite_post_update)(Sprite *self, uint32_t elapsed);
  // 确定最终状态，例如奔跑、待机、跳跃、下落、死亡
  void (*sprite_render)(Sprite *self, uint32_t elapsed);
};

typedef struct {
  float x, y;
  float w, h;
} DrawRect;

typedef struct {
  int sprite_index;
  int object_id;
  DrawRect rect;
  CollisionOffset hitbox_offset;
  gboolean active;
  float push_weight;
  // 本帧期望位移
  float desired_dx;
  float desired_dy;

  // 本帧碰撞解决后的实际位移
  float resolved_dx;
  float resolved_dy;
} SpriteData;

void sprite_pre_update(Sprite *self, uint32_t elapsed);

void sprite_post_update(Sprite *self, uint32_t elapsed);

void sprite_render(Sprite *self, uint32_t elapsed);

void spritelize(Sprite *self, TLN_Object *object, TLN_Spriteset sprite_set,
                TLN_Sequence seq);

bool deactive_sprite(Sprite *self);

SpriteData *get_sprite_data(Sprite *self);

HitBox get_sprite_hitbox(Sprite *self);

G_END_DECLS
#endif
