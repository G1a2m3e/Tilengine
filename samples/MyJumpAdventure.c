#include "Tilengine.h"
#include <string.h>

#define SCREEN_WIDTH 35 * 16
#define SCREEN_HEIGHT 15 * 16

int main(void) {
  TLN_Tilemap maps;
  TLN_SetLoadPath("assets/my_jump_adventure");
  TLN_Init(SCREEN_WIDTH, SCREEN_HEIGHT, 4, 0, 0);
  TLN_LoadWorld("my_jump_adventure.tmx", 0);
  TLN_CreateWindow(NULL, 0);
  while (TLN_ProcessWindow())
    TLN_DrawFrame(0);

  TLN_Deinit();
  return 0;
}