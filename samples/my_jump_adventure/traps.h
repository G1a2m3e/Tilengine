#ifndef TRAPS_H
#define TRAPS_H

#include "../../src/ObjectList.h"
#include "Tilengine.h"
#include "collision_handler.h"
#include "player.h"
#include <glib-object.h>
#include <stdint.h>

G_BEGIN_DECLS

typedef enum {
  FALLING_PLATFORM = 100,
  FIRST_TRAP_TYPE = FALLING_PLATFORM,
  LAST_TRAP_TYPE
} TRAP_TYPE;

#define T_TYPE_TRAP (t_trap_get_type())

G_DECLARE_DERIVABLE_TYPE(TTrap, t_trap, T, TRAP, GObject)

struct _TTrapClass {
  GObjectClass parent_class;
  // 虚函数，允许子类重写
  void (*update)(TTrap *self, Player *player, uint32_t elapsed);
  void (*render)(TTrap *self, Player *player, uint32_t elapsed);
};

typedef struct {
  HitBox hitbox;
  int sprite_index;
} TTrapData;

typedef struct {
  TTrapData data;
} TTrapPrivate;

// 公开API，供外部调用
void t_trap_factory_init(void);
TTrap *t_trap_obtain(int type, TLN_Object *obj);
TTrapData *t_trap_get_data(TTrap *self);
void t_trap_update(TTrap *self, Player *player, uint32_t elapsed);
void t_trap_render(TTrap *self, Player *player, uint32_t elapsed);

G_END_DECLS
#endif