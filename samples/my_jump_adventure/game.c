#include "game.h"
#include "../../src/Layer.h"
#include "../../src/LoadTMX.h"
#include "../../src/ObjectList.h"
#include "Tilengine.h"
#include "collision_handler.h"
#include "env_interaction.h"
#include "fruit.h"
#include "player.h"
#include "sprite.h"
#include "traps.h"
#include "utils.h"
#include <glib.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static Player *player;
static Fruit *fruits;
static int fruit_count;
static uint32_t last_update_ms;
static uint32_t last_render_ms;
static float background_x = 0.0f;
static int background_layer = -1;
static int world_width;
static int world_height;
static int camera_y;
static float camera_y_position;
static uint32_t last_camera_ms;
static bool camera_initialized;

static int clamp_camera(float position, int world_size, int screen_size) {
  int maximum = world_size - screen_size;
  if (maximum <= 0 || position <= 0)
    return 0;
  if (position >= maximum)
    return maximum;
  return (int)position;
}

static void update_camera(void) {
  SpriteData *data = get_sprite_data(T_SPRITE(player));
  float center_x = data->rect.x + data->hitbox_offset.x_offset +
                   data->hitbox_offset.w * 0.5f;
  float center_y = data->rect.y + data->hitbox_offset.y_offset +
                   data->hitbox_offset.h * 0.5f;
  int camera_x = clamp_camera(center_x - TLN_GetWidth() * 0.5f, world_width,
                              TLN_GetWidth());
  int target_y = clamp_camera(center_y - TLN_GetHeight() * 0.5f, world_height,
                              TLN_GetHeight());
  uint32_t now = TLN_GetTicks();
  if (!camera_initialized) {
    camera_y_position = target_y;
    camera_initialized = true;
  } else {
    float dt = fminf((now - last_camera_ms) / 1000.0f, 0.05f);
    camera_y_position +=
        (target_y - camera_y_position) * (1.0f - expf(-8.0f * dt));
  }
  last_camera_ms = now;
  camera_y = (int)roundf(camera_y_position);
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

static void init_player(const char *player_name) {
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
  player = player_new(player_obj);
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

  init_player(level_info.player_name);
  init_fruits(level_info.fruits_objectgroup_name);
  init_env_interaction();
  camera_initialized = false;
  update_camera();
}

static void check_fruit_collection(void) {
  SpriteData *data = get_sprite_data(T_SPRITE(player));
  HitBox player_box = {.x = data->rect.x + data->hitbox_offset.x_offset,
                       .y = data->rect.y + data->hitbox_offset.y_offset,
                       .w = data->hitbox_offset.w,
                       .h = data->hitbox_offset.h};

  for (int i = 0; i < fruit_count; ++i) {
    update_fruit(&fruits[i]);
    if (!fruits[i].collected && is_colliding(player_box, fruits[i].hitbox)) {
      collect(&fruits[i]);
    }
  }
}

static void update_traps(Player *player, uint32_t elapsed) {
  GPtrArray *traps = get_traps_array();
  for (guint i = 0; i < traps->len; ++i)
    t_trap_update(g_ptr_array_index(traps, i), player, elapsed);
}

static void pre_update_trap(uint32_t elapsed) {}

static void env_interaction(uint32_t elapsed) {
  SpriteData *player_data = get_sprite_data(T_SPRITE(player));
  HitBox player_hb = get_sprite_hitbox(T_SPRITE(player));
  HitBox result = handle_maps_collision(player_hb, player_data->desired_dx,
                                        player_data->desired_dy);
  player_data->resolved_dx = result.x - player_hb.x;
  player_data->resolved_dy = result.y - player_hb.y;
}

static void post_update_trap(uint32_t elapsed) {}

static void update_background(uint32_t elapsed) {
  if (background_layer >= 0) {
    background_x -= 20.0f * elapsed / 1000.0f;
    Layer *background = GetLayer(background_layer);
    background->world.offsetx = -(int)background_x;
    background->dirty = true;
  }
}

static bool finish_level() {
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
    return true;
  }
  return false;
}

bool update(uint32_t frame) {
  if (fruit_count == 0)
    return false;

  uint32_t now = TLN_GetTicks();
  uint32_t elapsed = now - last_update_ms;
  last_update_ms = now;

  sprite_pre_update(T_SPRITE(player), elapsed);
  pre_update_trap(elapsed);

  env_interaction(elapsed);

  sprite_post_update(T_SPRITE(player), elapsed);
  post_update_trap(elapsed);

  if (finish_level()) {
    return false;
  }

  update_background(elapsed);

  return true;
}

static void render_traps(uint32_t elapsed) {
  // GPtrArray *traps = get_traps_array();
  // for (guint i = 0; i < traps->len;) {
  //   TTrap *trap = g_ptr_array_index(traps, i);
  //   if (T_IS_FALLING_PLATFORM(trap) &&
  //       t_falling_platform_is_offscreen(T_FALLING_PLATFORM(trap),
  //                                       camera_y + TLN_GetHeight()))
  //     t_trap_deactivate(trap);
  //   if (!t_trap_get_data(trap)->active) {
  //     g_ptr_array_remove_index(traps, i);
  //     continue;
  //   }
  //   t_trap_render(trap, &player, elapsed);
  //   ++i;
  // }
}

void render(uint32_t frame) {
  uint32_t now = TLN_GetTicks();
  uint32_t elapsed = now - last_render_ms;
  last_render_ms = now;
  sprite_render(T_SPRITE(player), elapsed);
  render_traps(elapsed);
  update_camera();
}
