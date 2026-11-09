#include "traps.h"
#include "collision_handler.h"
#include "falling_platform.h"
#include <math.h>

G_DEFINE_TYPE_WITH_PRIVATE(TTrap, t_trap, G_TYPE_OBJECT)

static void t_trap_finalize(GObject *object) {
  TTrapPrivate *priv = t_trap_get_instance_private(T_TRAP(object));
  if (priv->data.sprite_index >= 0) {
    TLN_DisableSpriteAnimation(priv->data.sprite_index);
    TLN_DisableSprite(priv->data.sprite_index);
  }
  G_OBJECT_CLASS(t_trap_parent_class)->finalize(object);
}

static void t_trap_class_init(TTrapClass *klass) {
  G_OBJECT_CLASS(klass)->finalize = t_trap_finalize;
}
static GHashTable *trap_types = NULL;
static GHashTable *trap_animation = NULL;
typedef struct {
  const char *file;
  const char *prefix;
  TLN_Spriteset set;
  TLN_Sequence sequence;
} TrapAnimation;

static TrapAnimation falling_animation = {
    .file = "Traps/Falling Platforms/On (32x10).png",
    .prefix = "on",
};

static void t_trap_init(TTrap *self) {
  TTrapPrivate *priv = t_trap_get_instance_private(self);
  priv->data.sprite_index = -1;
}

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

void t_trap_update(TTrap *self, Player *player, uint32_t elapsed) {
  g_return_if_fail(T_IS_TRAP(self));

  TTrapClass *klass = T_TRAP_GET_CLASS(self);

  if (klass->update)
    klass->update(self, player, elapsed);
}

void t_trap_render(TTrap *self, Player *player, uint32_t elapsed) {
  g_return_if_fail(T_IS_TRAP(self));

  TTrapClass *klass = T_TRAP_GET_CLASS(self);

  if (klass->render)
    klass->render(self, player, elapsed);
}

TTrapData *t_trap_get_data(TTrap *self) {
  g_return_val_if_fail(T_IS_TRAP(self), NULL);

  TTrapPrivate *priv = t_trap_get_instance_private(self);
  return &priv->data;
}

TTrap *t_trap_obtain(int type, TLN_Object *obj) {
  if (trap_types == NULL || type < FIRST_TRAP_TYPE || type >= LAST_TRAP_TYPE)
    return NULL;

  GType gtype = (GType)GPOINTER_TO_SIZE(
      g_hash_table_lookup(trap_types, GINT_TO_POINTER(type)));

  if (gtype == G_TYPE_INVALID)
    return NULL;
  TTrap *trap = T_TRAP(g_object_new(gtype, NULL));
  TTrapPrivate *priv = t_trap_get_instance_private(trap);
  HitBox hb = {.x = obj->x,
               .y = obj->y,
               .w = obj->width,
               .h = obj->height,
               .push_weight = INFINITY};
  priv->data.hitbox = hb;

  TrapAnimation *animation =
      g_hash_table_lookup(trap_animation, GINT_TO_POINTER(type));
  if (!animation)
    goto failed;
  if (!animation->set)
    animation->set = TLN_LoadSpriteset(animation->file);
  if (!animation->set)
    goto failed;
  if (!animation->sequence)
    animation->sequence = TLN_CreateSpriteSequence(
        animation->prefix, animation->set, animation->prefix, 3);
  if (!animation->sequence)
    goto failed;

  priv->data.sprite_index = TLN_GetAvailableSprite();
  if (priv->data.sprite_index < 0)
    goto failed;
  if (!TLN_SetSpriteSet(priv->data.sprite_index, animation->set) ||
      !TLN_SetSpriteFlags(priv->data.sprite_index, obj->flags) ||
      !TLN_SetSpriteWorldPosition(priv->data.sprite_index, obj->x, obj->y) ||
      !TLN_SetSpriteAnimation(priv->data.sprite_index, animation->sequence, 0))
    goto failed;

  obj->visible = false;
  return trap;

failed:
  g_object_unref(trap);
  return NULL;
}