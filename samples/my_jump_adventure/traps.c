#include "traps.h"
#include "collision_handler.h"
#include "falling_platform.h"
#include <math.h>

G_DEFINE_TYPE_WITH_PRIVATE(TTrap, t_trap, T_TYPE_SPRITE)

static GHashTable *trap_types = NULL;
static GHashTable *trap_animation = NULL;
typedef struct {
  const char *file;
  const char *prefix;
  TLN_Spriteset set;
  TLN_Sequence sequence;
} TrapAnimation;

static void t_trap_finalize(GObject *object) {
  deactive_sprite(T_SPRITE(object));
  G_OBJECT_CLASS(t_trap_parent_class)->finalize(object);
}

static void t_trap_class_init(TTrapClass *klass) {
  G_OBJECT_CLASS(klass)->finalize = t_trap_finalize;
}

static TrapAnimation falling_animation = {
    .file = "Traps/Falling Platforms/On (32x10).png",
    .prefix = "on",
};

static void t_trap_init(TTrap *self) {}

void t_trap_factory_init(void) {
  if (trap_types)
    return;
  trap_types = g_hash_table_new(g_direct_hash, g_direct_equal);
  trap_animation = g_hash_table_new(g_direct_hash, g_direct_equal);

  g_hash_table_insert(trap_types, GINT_TO_POINTER(FALLING_PLATFORM),
                      GSIZE_TO_POINTER(T_TYPE_FALLING_PLATFORM));
  g_hash_table_insert(trap_animation, GINT_TO_POINTER(FALLING_PLATFORM),
                      &falling_animation);
}

TTrapData *t_trap_get_data(TTrap *self) {
  g_return_val_if_fail(T_IS_TRAP(self), NULL);

  TTrapPrivate *priv = t_trap_get_instance_private(self);
  return &priv->data;
}

TTrap *t_trap_obtain(int type, TLN_Object *obj) {
  GType gtype = (GType)GPOINTER_TO_SIZE(
      g_hash_table_lookup(trap_types, GINT_TO_POINTER(type)));
  TrapAnimation *anim =
      g_hash_table_lookup(trap_animation, GINT_TO_POINTER(type));
  if (!anim->set)
    anim->set = TLN_LoadSpriteset(anim->file);
  if (!anim->sequence)
    anim->sequence = TLN_CreateSpriteSequence(anim->prefix, anim->set, anim->prefix, 3);

  TTrap *trap = g_object_new(gtype, NULL);
  Sprite *sprite = T_SPRITE(trap);
  spritelize(sprite, obj, anim->set, anim->sequence);
  SpriteData *data = get_sprite_data(sprite);

  data->hitbox_offset = (CollisionOffset){0, 0, obj->width, obj->height};
  data->push_weight = INFINITY;
  t_trap_get_data(trap)->previous_hitbox = get_sprite_hitbox(sprite);
  return trap;
}
