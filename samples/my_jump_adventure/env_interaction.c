#include "env_interaction.h"
#include "../../src/ObjectList.h"
#include "Tilengine.h"
#include "collision_handler.h"
#include "falling_platform.h"
#include "glib-object.h"
#include "player.h"
#include "traps.h"
#include <math.h>

static GPtrArray *traps = NULL;
static const float CONTACT_EPSILON = 0.001f;

static void init_traps() {
  if (traps != NULL) {
    g_ptr_array_unref(traps);
    traps = NULL;
  }
  traps = g_ptr_array_new_with_free_func(g_object_unref);
  for (int i = 0; i < TLN_GetNumLayers(); ++i) {
    TLN_ObjectList list = TLN_GetLayerObjects(i);
    if (!list)
      continue;
    for (TLN_Object *obj = list->list; obj; obj = obj->next) {
      if (obj->type >= FIRST_TRAP_TYPE && obj->type < LAST_TRAP_TYPE) {
        TTrap *trap = t_trap_obtain(obj->type, obj);
        if (trap)
          g_ptr_array_add(traps, trap);
      }
    }
  }
}

GPtrArray *get_traps_array() { return traps; }

void init_env_interaction() { init_traps(); }

static bool is_player_grounded_with_map(Player *player) {
  HitBox box = (HitBox){
      .x = player->x + player->hitbox_offset.x_offset,
      .y = player->y + player->hitbox_offset.y_offset,
      .w = player->hitbox_offset.w,
      .h = player->hitbox_offset.h,
  };
  TLN_Tilemap tilemap = get_collision_tilemap();
  if (!tilemap)
    return false;
  int tw = TLN_GetTileWidth(tilemap->tilesets[0]);
  int th = TLN_GetTileHeight(tilemap->tilesets[0]);

  float bottom = box.y + box.h;
  int row = (int)floorf(bottom / th);
  if (row < 0 || row >= tilemap->rows || bottom != row * th)
    return false;

  int first = (int)floorf(box.x / tw);
  int last = (int)ceilf((box.x + box.w) / tw) - 1;
  if (first < 0)
    first = 0;
  if (last >= tilemap->cols)
    last = tilemap->cols - 1;

  for (int col = first; col <= last; ++col)
    if (tilemap->tiles[row * tilemap->cols + col].index != 0)
      return true;

  return false;
}

static TTrap *get_supporting_platform(Player *player) {
  if (!traps || player->velocity_y < 0)
    return NULL;

  float left = player->x + player->hitbox_offset.x_offset;
  float right = left + player->hitbox_offset.w;
  float bottom =
      player->y + player->hitbox_offset.y_offset + player->hitbox_offset.h;
  for (guint i = 0; i < traps->len; ++i) {
    TTrap *trap = g_ptr_array_index(traps, i);
    if (!T_IS_FALLING_PLATFORM(trap))
      continue;
    HitBox platform = t_trap_get_data(trap)->hitbox;
    if (left < platform.x + platform.w && right > platform.x &&
        fabsf(bottom - platform.y) <= CONTACT_EPSILON)
      return trap;
  }
  return NULL;
}

void update_env_interaction(Player *player, uint32_t elapsed) {
  if (!traps || !player)
    return;

  // Detect support at the old positions, before any platform moves.
  TTrap *support = get_supporting_platform(player);
  for (guint i = 0; i < traps->len; ++i)
    t_trap_update(g_ptr_array_index(traps, i), player, elapsed);

  if (support) {
    TTrapData *data = t_trap_get_data(support);
    HitBox box = {.x = player->x + player->hitbox_offset.x_offset,
                  .y = player->y + player->hitbox_offset.y_offset,
                  .w = player->hitbox_offset.w,
                  .h = player->hitbox_offset.h};
    float dx = data->hitbox.x - data->previous_hitbox.x;
    float dy = data->hitbox.y - (box.y + box.h);
    HitBox result = handle_maps_collision(box, dx, dy);
    player->x = result.x - player->hitbox_offset.x_offset;
    player->y = result.y - player->hitbox_offset.y_offset;
  }
}

bool is_player_grounded(Player *player) {
  if (is_player_grounded_with_map(player)) {
    return true;
  }
  if (get_supporting_platform(player)) {
    return true;
  }
  return false;
}

bool is_touching_wall(HitBox box) {
  TLN_Tilemap tilemap = get_collision_tilemap();
  if (!tilemap)
    return false;
  int tw = TLN_GetTileWidth(tilemap->tilesets[0]);
  int th = TLN_GetTileHeight(tilemap->tilesets[0]);

  int first = (int)floorf(box.y / th);
  int last = (int)ceilf((box.y + box.h) / th) - 1;
  if (first < 0)
    first = 0;
  if (last >= tilemap->rows)
    last = tilemap->rows - 1;

  int left = (int)floorf(box.x / tw) - 1;
  float right_edge = box.x + box.w;
  int right = (int)floorf(right_edge / tw);
  bool check_left =
      left >= 0 && left < tilemap->cols && box.x == (left + 1) * tw;
  bool check_right =
      right >= 0 && right < tilemap->cols && right_edge == right * tw;

  for (int row = first; row <= last; ++row) {
    if ((check_left && tilemap->tiles[row * tilemap->cols + left].index != 0) ||
        (check_right && tilemap->tiles[row * tilemap->cols + right].index != 0))
      return true;
  }
  return false;
}

HitBox handle_traps_collision(HitBox src, float dx, float dy) {
  HitBox result = src;
  result.x += dx;
  result.y += dy;
  if (!traps)
    return result;

  float bottom = src.y + src.h;
  float nearest = 1.0f;
  for (guint i = 0; i < traps->len; ++i) {
    TTrap *trap = g_ptr_array_index(traps, i);
    if (!T_IS_FALLING_PLATFORM(trap))
      continue;
    TTrapData *data = t_trap_get_data(trap);
    HitBox previous = data->previous_hitbox;
    float platform_dx = data->hitbox.x - previous.x;
    float platform_dy = data->hitbox.y - previous.y;
    float relative_dy = dy - platform_dy;
    // Sweep against the one-way top in the platform's reference frame.
    // A rising platform can catch a stationary player too.
    if (relative_dy <= 0 || bottom > previous.y + CONTACT_EPSILON)
      continue;
    float hit_t = fmaxf(0.0f, (previous.y - bottom) / relative_dy);
    float left = src.x + (dx - platform_dx) * hit_t;
    if (hit_t <= nearest && left < previous.x + previous.w &&
        left + src.w > previous.x) {
      nearest = hit_t;
      result.y = data->hitbox.y - src.h;
    }
  }
  return result;
}
