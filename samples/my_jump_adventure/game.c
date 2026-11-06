#include "game.h"
#include "../../src/ObjectList.h"
#include "Tilengine.h"
#include "fruit.h"
#include "player.h"
#include "utils.h"
#include <stdlib.h>

static Player player;
static Fruit *fruits;
static int fruit_count;
static uint32_t last_update_ms;

static void init_fruits(void) {
  for (int i = 0; i < fruit_count; ++i) {
    if (fruits[i].collect_sprite >= 0) {
      TLN_DisableSpriteAnimation(fruits[i].collect_sprite);
      TLN_DisableSprite(fruits[i].collect_sprite);
    }
  }
  free(fruits);
  fruits = NULL;
  fruit_count = 0;

  TLN_ObjectList list = find_object_list(20);
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

void init_game() {
  last_update_ms = TLN_GetTicks();
  init_player(&player, "virtual_guy");
  init_fruits();
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

void update(uint32_t frame) {
  uint32_t now = TLN_GetTicks();
  uint32_t elapsed = now - last_update_ms;
  last_update_ms = now;
  update_player(&player, elapsed);
  check_fruit_collection();
}

void render(uint32_t frame) { render_player(&player, frame); }
