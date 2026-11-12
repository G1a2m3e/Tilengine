#include "env_interaction.h"
#include "../../src/ObjectList.h"
#include "Tilengine.h"
#include "collision_handler.h"
#include "falling_platform.h"
#include "glib-object.h"
#include "player.h"
#include "saw.h"
#include "sprite.h"
#include "traps.h"
#include <math.h>

static GPtrArray *traps = NULL;
static Player *player;
static TTrap *get_supporting_platform(Player *player);

void init_traps() {
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

Player *get_player() { return player; }

void init_player(const char *player_name) {
  TLN_Object *player_obj = NULL;

  for (int layer = 0; layer < TLN_GetNumLayers() && !player_obj; ++layer) {
    TLN_ObjectList objects = TLN_GetLayerObjects(layer);
    if (!objects)
      continue;

    for (TLN_Object *obj = objects->list; obj; obj = obj->next) {
      if (strcmp(obj->name, player_name) == 0) {
        player_obj = obj;
        break;
      }
    }
  }
  if (player) {
    deactive_sprite(T_SPRITE(player));
    g_clear_object(&player);
  }
  player = player_new(player_obj);
}

static bool hits_saw(HitBox box, float dx, float dy) {
  if (!traps)
    return false;
  for (guint i = 0; i < traps->len; ++i) {
    TTrap *trap = g_ptr_array_index(traps, i);
    SpriteData *data = get_sprite_data(T_SPRITE(trap));
    if (!T_IS_SAW(trap) || !data->active)
      continue;
    Circle circle = {data->rect.x + data->rect.w * 0.5f,
                     data->rect.y + data->rect.h * 0.5f, data->rect.w * 0.5f};
    float hit_t;
    if (swept_aabb_rect_circle(box, circle, dx - data->desired_dx,
                               dy - data->desired_dy, &hit_t))
      return true;
  }
  return false;
}

bool update_env_interaction(uint32_t elapsed) {
  if (!player)
    return false;

  SpriteData *player_data = get_sprite_data(T_SPRITE(player));
  HitBox box = get_sprite_hitbox(T_SPRITE(player));
  float dx = player_data->desired_dx;
  float dy = player_data->desired_dy;
  if (hits_saw(box, dx, dy)) {
    die(player);
    return true;
  }

  bool supported = false;

  if (traps) {
    for (guint i = 0; i < traps->len; ++i) {
      TTrap *trap = g_ptr_array_index(traps, i);
      SpriteData *trap_data = get_sprite_data(T_SPRITE(trap));
      if (!trap_data->active)
        continue;
      if (T_IS_FALLING_PLATFORM(trap)) {
        if (!supported && player_get_velocity_y(player) >= 0)
          supported =
              t_falling_platform_carry(T_FALLING_PLATFORM(trap), box, &dx, &dy);
      }
    }
  }

  HitBox result = handle_maps_collision(box, dx, dy);
  if (!supported)
    result = handle_traps_collision(box, result.x - box.x, result.y - box.y);
  player_data->resolved_dx = result.x - box.x;
  player_data->resolved_dy = result.y - box.y;
  return false;
}

static bool is_player_grounded_with_map(Player *player) {
  SpriteData *data = get_sprite_data(T_SPRITE(player));
  HitBox box = (HitBox){
      .x = data->rect.x + data->hitbox_offset.x_offset,
      .y = data->rect.y + data->hitbox_offset.y_offset,
      .w = data->hitbox_offset.w,
      .h = data->hitbox_offset.h,
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
  if (!traps || player_get_velocity_y(player) < 0)
    return NULL;

  HitBox box = get_sprite_hitbox(T_SPRITE(player));
  for (guint i = 0; i < traps->len; ++i) {
    TTrap *trap = g_ptr_array_index(traps, i);
    if (!T_IS_FALLING_PLATFORM(trap))
      continue;
    if (!get_sprite_data(T_SPRITE(trap))->active)
      continue;
    if (t_falling_platform_is_supporting(T_FALLING_PLATFORM(trap), box))
      return trap;
  }
  return NULL;
}

bool is_player_grounded(void) {
  if (!player)
    return false;
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

static void notify_trap_contact(TTrap *trap, HitBox box) {
  if (T_IS_FALLING_PLATFORM(trap)) {
    TFallingPlatform *platform = T_FALLING_PLATFORM(trap);
    if (t_falling_platform_is_supporting(platform, box))
      t_falling_platform_touch(platform);
  }
}

HitBox handle_traps_collision(HitBox src, float dx, float dy) {
  HitBox result = src;
  result.x += dx;
  result.y += dy;
  if (!traps)
    return result;

  float nearest = 1.0f;
  TTrap *landing = NULL;
  for (guint i = 0; i < traps->len; ++i) {
    TTrap *trap = g_ptr_array_index(traps, i);
    if (!T_IS_FALLING_PLATFORM(trap))
      continue;
    float hit_t;
    HitBox candidate;
    if (t_falling_platform_check_collision(T_FALLING_PLATFORM(trap), src, dx,
                                           dy, &hit_t, &candidate) &&
        hit_t <= nearest) {
      nearest = hit_t;
      result = candidate;
      landing = trap;
    }
  }
  if (landing) {
    // 平台修正落点后仍需受地图约束，防止被抬进天花板。
    result = handle_maps_collision(src, result.x - src.x, result.y - src.y);
    notify_trap_contact(landing, result);
  }
  return result;
}
