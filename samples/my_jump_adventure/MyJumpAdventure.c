#include "../../src/LoadTMX.h"
#include "Tilengine.h"
#include "game.h"
#include <stdio.h>
#include <string.h>

#define TMX_FILE_NAME "my_jump_adventure.tmx"

static void main_loop(uint32_t frame);

int main(void) {
  TLN_SetLoadPath("assets/my_jump_adventure");

  TMXInfo info = {0};
  if (!TMXLoad(TMX_FILE_NAME, &info)) {
    fprintf(stderr, "读取地图失败\n");
    return 1;
  }

  int num_sprites = 0;
  for (int i = 0; i < info.num_layers; ++i) {
    if (info.layers[i].type == LAYER_OBJECT)
      num_sprites += info.layers[i].num_objects;
  }
  if (!TLN_Init(info.width * info.tilewidth, info.height * info.tileheight,
                info.num_layers, num_sprites, 0)) {
    fprintf(stderr, "Failed to initialize Tilengine\n");
    return 1;
  }
  TLN_LoadWorld(TMX_FILE_NAME, 0);
  init_game();

  TLN_CreateWindow(NULL, 0);
  TLN_SetMainTask(main_loop);
  return 0;
}

static void main_loop(uint32_t frame) {
  update(frame);
  render(frame);
}
