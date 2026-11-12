#include "traps.h"
#include "../../src/Sequence.h"
#include "../../src/Tileset.h"
#include <string.h>
#include "collision_handler.h"
#include "falling_platform.h"
#include "rock_head.h"
#include "saw.h"
#include <math.h>

G_DEFINE_TYPE_WITH_PRIVATE(TTrap, t_trap, T_TYPE_SPRITE)

static GHashTable *trap_types = NULL;
static GHashTable *trap_sets = NULL; // One generated atlas per source tileset GUID.

static void t_trap_finalize(GObject *object) {
  deactive_sprite(T_SPRITE(object));
  TTrapPrivate *priv = t_trap_get_instance_private(T_TRAP(object));
  if (priv->sequence)
    TLN_DeleteSequence(priv->sequence);
  G_OBJECT_CLASS(t_trap_parent_class)->finalize(object);
}

static void t_trap_class_init(TTrapClass *klass) {
  G_OBJECT_CLASS(klass)->finalize = t_trap_finalize;
}

static void t_trap_init(TTrap *self) {}

void t_trap_factory_init(void) {
  if (trap_types)
    return;
  trap_types = g_hash_table_new(g_direct_hash, g_direct_equal);
  trap_sets = g_hash_table_new(g_direct_hash, g_direct_equal);
  g_hash_table_insert(trap_types, GINT_TO_POINTER(FALLING_PLATFORM),
                      GSIZE_TO_POINTER(T_TYPE_FALLING_PLATFORM));
  g_hash_table_insert(trap_types, GINT_TO_POINTER(SAW),
                      GSIZE_TO_POINTER(T_TYPE_SAW));
  g_hash_table_insert(trap_types, GINT_TO_POINTER(ROCK_HEAD),
                      GSIZE_TO_POINTER(T_TYPE_ROCKHEAD));
}

TTrapData *t_trap_get_data(TTrap *self) {
  g_return_val_if_fail(T_IS_TRAP(self), NULL);

  TTrapPrivate *priv = t_trap_get_instance_private(self);
  return &priv->data;
}

static TLN_Spriteset get_trap_spriteset(TLN_Tileset tileset) {
  TLN_Spriteset set = g_hash_table_lookup(trap_sets, GINT_TO_POINTER(tileset->guid));
  if (set)
    return set;
  if (tileset->tstype != TILESET_TILES || tileset->numtiles <= 0)
    return NULL;

  int w = tileset->width, h = tileset->height, count = tileset->numtiles;
  TLN_Bitmap bitmap = TLN_CreateBitmap(w * count, h, 8);
  if (!bitmap)
    return NULL;
  TLN_SetBitmapPalette(bitmap, TLN_ClonePalette(tileset->palette));
  TLN_SpriteData *entries = g_new0(TLN_SpriteData, count);
  for (int i = 0; i < count; ++i) {
    entries[i] = (TLN_SpriteData){.x = i * w, .w = w, .h = h};
    for (int y = 0; y < h; ++y)
      memcpy(TLN_GetBitmapPtr(bitmap, i * w, y),
             &GetTilesetPixel(tileset, i, 0, y), w);
  }
  set = TLN_CreateSpriteset(bitmap, entries, count);
  g_free(entries);
  if (!set) {
    TLN_DeleteBitmap(bitmap);
    return NULL;
  }
  g_hash_table_insert(trap_sets, GINT_TO_POINTER(tileset->guid), set);
  return set;
}

static TLN_Sequence create_trap_sequence(TLN_Object *obj) {
  TLN_SequencePack pack = TLN_GetTilesetSequencePack(obj->tileset);
  char name[16];
  snprintf(name, sizeof(name), "%u", obj->gid);
  TLN_Sequence source = pack ? TLN_FindSequence(pack, name) : NULL;
  if (!source) {
    TLN_SequenceFrame still = {.index = obj->gid, .delay = 1};
    return TLN_CreateSequence("still", 0, 1, &still);
  }

  TLN_Sequence sequence = TLN_CloneSequence(source);
  if (!sequence)
    return NULL;
  TLN_SequenceFrame *frames = (TLN_SequenceFrame *)sequence->data;
  for (int i = 0; i < sequence->count; ++i)
    --frames[i].index; // TSX frames are one-based; sprites are zero-based.
  return sequence; // Preserve TSX frame order and individual delays.
}

TTrap *t_trap_obtain(int type, TLN_Object *obj) {
  GType gtype = (GType)GPOINTER_TO_SIZE(
      g_hash_table_lookup(trap_types, GINT_TO_POINTER(type)));
  TLN_Spriteset set = get_trap_spriteset(obj->tileset);
  if (!set)
    return NULL;

  TTrap *trap = g_object_new(gtype, NULL);
  TTrapPrivate *priv = t_trap_get_instance_private(trap);
  priv->sequence = create_trap_sequence(obj);
  if (!priv->sequence) {
    g_object_unref(trap);
    return NULL;
  }
  Sprite *sprite = T_SPRITE(trap);
  spritelize(sprite, obj, set, priv->sequence);
  SpriteData *data = get_sprite_data(sprite);
  TLN_SequenceFrame *frames = (TLN_SequenceFrame *)priv->sequence->data;
  TLN_SetSpritePicture(data->sprite_index, frames[0].index);
  data->hitbox_offset = (CollisionOffset){0, 0, obj->width, obj->height};
  data->push_weight = INFINITY;
  priv->data.previous_hitbox = get_sprite_hitbox(sprite);
  return trap;
}
