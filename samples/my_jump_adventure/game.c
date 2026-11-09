#include "game.h"
#include "../../src/Layer.h"
#include "../../src/LoadTMX.h"
#include "../../src/ObjectList.h"
#include "Tilengine.h"
#include "collision_handler.h"
#include "env_interaction.h"
#include "fruit.h"
#include "player.h"
#include "traps.h"
#include "utils.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>

static Player player;
static Fruit *fruits;
static int fruit_count;
static uint32_t last_update_ms;
static uint32_t last_render_ms;
static float background_x = 0.0f;
static int background_layer = -1;
static int world_width;
static int world_height;

static int clamp_camera(float position, int world_size, int screen_size) {
  int maximum = world_size - screen_size;
  if (maximum <= 0 || position <= 0)
    return 0;
  if (position >= maximum)
    return maximum;
  return (int)position;
}

static void update_camera(void) {
  float center_x =
      player.x + player.hitbox_offset.x_offset + player.hitbox_offset.w * 0.5f;
  float center_y =
      player.y + player.hitbox_offset.y_offset + player.hitbox_offset.h * 0.5f;
  int camera_x = clamp_camera(center_x - TLN_GetWidth() * 0.5f, world_width,
                              TLN_GetWidth());
  int camera_y = clamp_camera(center_y - TLN_GetHeight() * 0.5f, world_height,
                              TLN_GetHeight());
  TLN_SetWorldPosition(camera_x, camera_y);
}

static void clear_fruits(void) {
  for (int i = 0; i < fruit_count; ++i) {
    if (fruits[i].collect_sprite >= 0) {
      TLN_DisableSpriteAnimation(fruits[i].collect_sprite);
      TLN_DisableSprite(fruits[i].collect_sprite);
    }
  }
  free(fruits);
  fruits = NULL;
  fruit_count = 0;
}

static void init_fruits(const char *fruits_objectgroup_name) {
  clear_fruits();

  TLN_ObjectList list = find_object_list_by_name(fruits_objectgroup_name);
  if (!list)
    return;

  int count = TLN_GetListNumObjects(list);
  if (count <= 0)
    return;

  fruits = calloc((size_t)count, sizeof(*fruits));
  if (!fruits)
    return;

  for (TLN_Object *obj = list->list; obj; obj = obj->next) {
    fruits[fruit_count++] = (Fruit){
        .hitbox = {.x = obj->x + 8, .y = obj->y + 7, .w = 14, .h = 14},
        .collected = false,
        .object = obj,
        .collect_sprite = -1,
    };
  }
}

void reset_game_timer(void) {
  last_update_ms = TLN_GetTicks();
  last_render_ms = TLN_GetTicks();
}

void init_game(int current_level, LevelInfo level_info) {
  TMXInfo map = {0};
  world_width = TLN_GetWidth();
  world_height = TLN_GetHeight();
  if (TMXLoad(get_level_file_name(current_level), &map)) {
    world_width = map.width * map.tilewidth;
    world_height = map.height * map.tileheight;
  }
  reset_game_timer();
  background_layer = get_background_layer();
  background_x = 0.0f;
  if (background_layer >= 0) {
    // Keep the existing autonomous background scroll independent of the
    // camera.
    TLN_SetLayerParallaxFactor(background_layer, 0.0f, 0.0f);
    Layer *background = GetLayer(background_layer);
    background->world.offsetx = 0;
    background->world.offsety = 0;
  }
  init_player(current_level, &player, level_info.player_name);
  init_fruits(level_info.fruits_objectgroup_name);
  init_env_interaction();
  update_camera();
}

static void check_fruit_collection(void) {
  HitBox player_box = {.x = player.x + player.hitbox_offset.x_offset,
                       .y = player.y + player.hitbox_offset.y_offset,
                       .w = player.hitbox_offset.w,
                       .h = player.hitbox_offset.h};

  for (int i = 0; i < fruit_count; ++i) {
    update_fruit(&fruits[i]);
    if (!fruits[i].collected && is_colliding(player_box, fruits[i].hitbox)) {
      collect(&fruits[i]);
    }
  }
}

bool update(uint32_t frame) {
  if (fruit_count == 0)
    return false;

  uint32_t now = TLN_GetTicks();
  uint32_t elapsed = now - last_update_ms;
  last_update_ms = now;
  update_player(&player, elapsed);
  GPtrArray *traps = get_traps_array();
  for (guint i = 0; i < traps->len; ++i) {
    TTrap *trap = g_ptr_array_index(traps, i);
    t_trap_update(trap, &player, elapsed);
  }
  check_fruit_collection();

  bool all_collected = true;
  for (int i = 0; i < fruit_count; ++i) {
    if (!fruits[i].collected) {
      all_collected = false;
      break;
    }
  }
  if (all_collected) {
    clear_fruits();
    return false;
  }

  if (background_layer >= 0) {
    background_x -= 20.0f * elapsed / 1000.0f;
    Layer *background = GetLayer(background_layer);
    background->world.offsetx = -(int)background_x;
    background->dirty = true;
  }
  return true;
}

void render(uint32_t frame) {
  uint32_t now = TLN_GetTicks();
  uint32_t elapsed = now - last_render_ms;
  last_render_ms = now;
  update_camera();
  render_player(&player, frame);
  GPtrArray *traps = get_traps_array();
  for (guint i = 0; i < traps->len; ++i) {
    TTrap *trap = g_ptr_array_index(traps, i);
    t_trap_render(trap, &player, elapsed);
  }
}
