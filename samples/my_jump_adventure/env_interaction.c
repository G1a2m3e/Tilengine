#include "env_interaction.h"
#include "../../src/ObjectList.h"
#include "Tilengine.h"
#include "collision_handler.h"
#include "falling_platform.h"
#include "glib-object.h"
#include "player.h"
#include "sprite.h"
#include "traps.h"
#include <math.h>

static GPtrArray *traps = NULL;
static Player *player;
static const float CONTACT_EPSILON = 0.001f;
static TTrap *get_supporting_platform(Player *player, bool previous);

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

void update_env_interaction(uint32_t elapsed) {
  SpriteData *data = get_sprite_data(T_SPRITE(player));
  HitBox box = get_sprite_hitbox(T_SPRITE(player));
  float dx = data->desired_dx;
  float dy = data->desired_dy;
  TTrap *support = get_supporting_platform(player, true);
  if (support) {
    HitBox platform = get_sprite_hitbox(T_SPRITE(support));
    HitBox previous = t_trap_get_data(support)->previous_hitbox;
    float carry_dx = platform.x - previous.x;
    float left = box.x + dx + carry_dx;
    if (left < platform.x + platform.w && left + box.w > platform.x) {
      dx += carry_dx;
      dy += platform.y - (box.y + box.h);
      t_falling_platform_touch(T_FALLING_PLATFORM(support));
    } else {
      support = NULL;
    }
  }

  HitBox result = handle_maps_collision(box, dx, dy);
  if (!support)
    result = handle_traps_collision(box, result.x - box.x, result.y - box.y);
  data->resolved_dx = result.x - box.x;
  data->resolved_dy = result.y - box.y;
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

static TTrap *get_supporting_platform(Player *player, bool previous) {
  SpriteData *data = get_sprite_data(T_SPRITE(player));
  if (!traps || player_get_velocity_y(player) < 0)
    return NULL;

  float left = data->rect.x + data->hitbox_offset.x_offset;
  float right = left + data->hitbox_offset.w;
  float bottom =
      data->rect.y + data->hitbox_offset.y_offset + data->hitbox_offset.h;
  for (guint i = 0; i < traps->len; ++i) {
    TTrap *trap = g_ptr_array_index(traps, i);
    if (!T_IS_FALLING_PLATFORM(trap))
      continue;
    if (!get_sprite_data(T_SPRITE(trap))->active)
      continue;
    HitBox platform = previous ? t_trap_get_data(trap)->previous_hitbox
                               : get_sprite_hitbox(T_SPRITE(trap));
    if (left < platform.x + platform.w && right > platform.x &&
        fabsf(bottom - platform.y) <= CONTACT_EPSILON)
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
  if (get_supporting_platform(player, false)) {
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
  TTrap *landing = NULL;
  for (guint i = 0; i < traps->len; ++i) {
    TTrap *trap = g_ptr_array_index(traps, i);
    if (!T_IS_FALLING_PLATFORM(trap) ||
        !get_sprite_data(T_SPRITE(trap))->active)
      continue;
    HitBox platform = get_sprite_hitbox(T_SPRITE(trap));
    HitBox previous = t_trap_get_data(trap)->previous_hitbox;
    float relative_dy = dy - (platform.y - previous.y);
    if (relative_dy <= 0 || bottom > previous.y + CONTACT_EPSILON)
      continue;

    float hit_t = fmaxf(0, (previous.y - bottom) / relative_dy);
    float left = src.x + (dx - (platform.x - previous.x)) * hit_t;
    if (hit_t <= nearest && left < previous.x + previous.w &&
        left + src.w > previous.x) {
      nearest = hit_t;
      result.y = platform.y - src.h;
      landing = trap;
    }
  }
  if (landing) {
    // 平台修正落点后仍需受地图约束，防止被抬进天花板。
    result = handle_maps_collision(src, result.x - src.x, result.y - src.y);
    HitBox platform = get_sprite_hitbox(T_SPRITE(landing));
    if (fabsf(result.y + src.h - platform.y) <= CONTACT_EPSILON)
      t_falling_platform_touch(T_FALLING_PLATFORM(landing));
  }
  return result;
}
