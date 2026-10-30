#include "../../src/LoadTMX.h"
#include "Tilengine.h"
#include "player.h"
#include <stdio.h>

#define SCREEN_WIDTH (35 * 16)
#define SCREEN_HEIGHT (15 * 16)
#define TMX_FILE_NAME "my_jump_adventure.tmx"
/* LoadWorld reverses TMX order: fruits=0, player=1, maps=2, background=3. */
#define PLAYER_LAYER 0

static Player player;
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
  if (!TLN_Init(SCREEN_WIDTH, SCREEN_HEIGHT, info.num_layers, num_sprites, 0)) {
    fprintf(stderr, "Failed to initialize Tilengine\n");
    return 1;
  }
  TLN_LoadWorld(TMX_FILE_NAME, 0);
  init_player(&player, "virtual_guy",
              "Main Characters/Virtual Guy/Idle (32x32).png");

  TLN_CreateWindow(NULL, 0);
  TLN_SetMainTask(main_loop);
  return 0;
}

static void main_loop(uint32_t frame) {
  if (TLN_GetInput(INPUT_LEFT))
    player_move_to_left(&player);
  if (TLN_GetInput(INPUT_RIGHT))
    player_move_to_right(&player);
}
