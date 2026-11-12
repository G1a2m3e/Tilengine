#ifndef TRAPS_H
#define TRAPS_H

#include "../../src/ObjectList.h"
#include "collision_handler.h"
#include "sprite.h"
#include <glib-object.h>
#include <stdint.h>

G_BEGIN_DECLS

typedef enum {
  FALLING_PLATFORM = 100,
  FIRST_TRAP_TYPE = FALLING_PLATFORM,
  SAW = 101,
  ROCK_HEAD = 102,
  LAST_TRAP_TYPE
} TRAP_TYPE;

#define T_TYPE_TRAP (t_trap_get_type())

G_DECLARE_DERIVABLE_TYPE(TTrap, t_trap, T, TRAP, Sprite)

struct _TTrapClass {
  SpriteClass parent_class;
};

typedef struct {
  HitBox previous_hitbox;
} TTrapData;

typedef struct {
  TTrapData data;
  TLN_Sequence sequence; // Sprite sequence copied from this object's TSX.
} TTrapPrivate;

// 公开API，供外部调用
void t_trap_factory_init(void);
TTrap *t_trap_obtain(int type, TLN_Object *obj);
TTrapData *t_trap_get_data(TTrap *self);

G_END_DECLS
#endif
