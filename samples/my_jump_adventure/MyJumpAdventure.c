#include "../../src/LoadTMX.h"
#include "Tilengine.h"
#include "game.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
  关卡设计要求:
  1. 玩家叫virtual_guy
  2. 水果单独放在对象层fruits中
*/
#define TMX_FILE_NAME "level1.tmx"

static int current_level = 0;
static void main_loop(uint32_t frame);

static void next_level() {
  current_level++;
  TMXInfo info = {0};
  LevelInfo level_info = {0};
  if (!TMXLoad(get_level_file_name(current_level), &info)) {
    fprintf(stderr, "读取地图失败\n");
    exit(1);
  }

  int num_sprites = 0;
  for (int i = 0; i < info.num_layers; ++i) {
    if (info.layers[i].type == LAYER_OBJECT)
      num_sprites += info.layers[i].num_objects;
  }
  if (!TLN_Init(info.width * info.tilewidth, info.height * info.tileheight,
                info.num_layers, num_sprites, 0)) {
    fprintf(stderr, "Failed to initialize Tilengine\n");
    exit(1);
  }
  TLN_LoadWorld(get_level_file_name(current_level), 0);

  level_info.player_name = "virtual_guy";
  level_info.fruits_objectgroup_name = "fruits";
  init_game(current_level, level_info);
}

void clear_all_sprites(void) {
  int count = TLN_GetNumSprites();

  for (int i = 0; i < count; ++i) {
    TLN_DisableSpriteAnimation(i);
    TLN_DisableSprite(i);
  }
}

int main(void) {
  TLN_SetLoadPath("assets/my_jump_adventure");

  next_level();

  TLN_CreateWindow(NULL, 0);
  TLN_SetMainTask(main_loop);
  return 0;
}

static void main_loop(uint32_t frame) {
  bool switch_level = !update(frame);
  if (switch_level) {
    clear_all_sprites();
    TLN_ReleaseWorld();
    next_level();
    return;
  }
  render(frame);
}
