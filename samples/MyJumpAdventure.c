#include "Tilengine.h"
#include <stdio.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 160
#define PLAYER_SPEED 3
#define PLAYER_SIZE 32
#define TILE_SIZE 16
#define GRAVITY 1
#define MAX_FALL_SPEED 8
/* player_Idle.tsx is the second tileset in my_tiled.tmx. */
#define PLAYER_TILESET 1

static int camera_x;
static int max_camera_x;
static int player_x;
static int player_y;
static int max_player_x;
static int velocity_y;
static TLN_Tilemap world_map;

/* Test the player's 32x32 rectangle against solid terrain in world coordinates.
 */
static bool hits_terrain(int x, int y) {
  int rows = TLN_GetTilemapRows(world_map);
  int cols = TLN_GetTilemapCols(world_map);
  if (x < 0 || y < 0 || x + PLAYER_SIZE > cols * TILE_SIZE ||
      y + PLAYER_SIZE > rows * TILE_SIZE)
    return true;
  for (int row = y / TILE_SIZE; row <= (y + PLAYER_SIZE - 1) / TILE_SIZE;
       row++) {
    for (int col = x / TILE_SIZE; col <= (x + PLAYER_SIZE - 1) / TILE_SIZE;
         col++) {
      Tile tile = {0};
      if (TLN_GetTilemapTile(world_map, row, col, &tile) && tile.index != 0 &&
          tile.tileset != PLAYER_TILESET)
        return true;
    }
  }
  return false;
}

/* Move one pixel at a time so a fast movement cannot skip a wall or floor. */
static void move_player(int direction) {
  for (int step = 0; step < PLAYER_SPEED && direction != 0; step++) {
    int next_x = player_x + direction;
    if (next_x < 0 || next_x > max_player_x || hits_terrain(next_x, player_y))
      break;
    player_x = next_x;
  }

  if (velocity_y < MAX_FALL_SPEED)
    velocity_y += GRAVITY;
  for (int step = 0; step < velocity_y; step++) {
    if (hits_terrain(player_x, player_y + 1)) {
      velocity_y = 0;
      break;
    }
    player_y++;
  }
}

static void update_positions(void) {
  camera_x = player_x + PLAYER_SIZE / 2 - SCREEN_WIDTH / 2;
  if (camera_x < 0)
    camera_x = 0;
  if (camera_x > max_camera_x)
    camera_x = max_camera_x;
  TLN_SetLayerPosition(0, camera_x, 0);
  TLN_SetSpritePosition(0, player_x - camera_x, player_y);
}

static void main_loop(uint32_t frame) {
  (void)frame;
  int direction = 0;
  if (TLN_GetInput(INPUT_LEFT))
    direction -= 1;
  if (TLN_GetInput(INPUT_RIGHT))
    direction += 1;

  move_player(direction);
  if (direction != 0)
    TLN_SetSpriteFlags(0, direction < 0 ? FLAG_FLIPX : 0);

  update_positions();
}

/* entry point */
int main(int argc, char *argv[]) {
  /* setup engine: 320x160, 1 layer and 1 player sprite */
  TLN_Init(SCREEN_WIDTH, SCREEN_HEIGHT, 1, 1, 0);
  TLN_SetBGColor(32, 32, 128);

  /* load resources */
  TLN_Tilemap tilemap = TLN_LoadTilemap("assets/my_jump_adventure/my_jump_adventure.tmx", NULL);
  if (tilemap == NULL) {
    fprintf(stderr, "Failed to load tilemap: %s\n",
            TLN_GetErrorString(TLN_GetLastError()));
    TLN_Deinit();
    return 1;
  }
  world_map = tilemap;

  /* setup the layer (index 0) */
  TLN_SetLayerTilemap(0, tilemap);
  TLN_SetLayerPosition(0, 0, 0);
  max_camera_x = TLN_GetLayerWidth(0) - SCREEN_WIDTH;
  if (max_camera_x < 0)
    max_camera_x = 0;
  max_player_x = TLN_GetLayerWidth(0) - PLAYER_SIZE;
  if (max_player_x < 0)
    max_player_x = 0;

  /* Use the top-left player tile as the spawn, then remove its map tiles. */
  bool found_player = false;
  for (int row = 0; row < TLN_GetTilemapRows(tilemap); row++) {
    for (int col = 0; col < TLN_GetTilemapCols(tilemap); col++) {
      Tile tile;
      TLN_GetTilemapTile(tilemap, row, col, &tile);
      if (tile.index != 0 && tile.tileset == PLAYER_TILESET) {
        if (!found_player) {
          player_x = col * TILE_SIZE;
          player_y = row * TILE_SIZE;
          found_player = true;
        }
        Tile empty = {0};
        TLN_SetTilemapTile(tilemap, row, col, &empty);
      }
    }
  }
  if (!found_player) {
    fprintf(stderr, "No player tiles found in the map\n");
    TLN_DeleteTilemap(tilemap);
    TLN_Deinit();
    return 1;
  }

  /* The first idle frame occupies the first 32x32 pixels of the sheet. */
  TLN_Bitmap bitmap = TLN_LoadBitmap("assets/my_jump_adventure/player_Idle.png");
  TLN_SpriteData sprite_data = {"player", 0, 0, PLAYER_SIZE, PLAYER_SIZE};
  TLN_Spriteset player =
      bitmap ? TLN_CreateSpriteset(bitmap, &sprite_data, 1) : NULL;
  if (player == NULL) {
    fprintf(stderr, "Failed to create player: %s\n",
            TLN_GetErrorString(TLN_GetLastError()));
    if (bitmap != NULL)
      TLN_DeleteBitmap(bitmap);
    TLN_DeleteTilemap(tilemap);
    TLN_Deinit();
    return 1;
  }
  TLN_ConfigSprite(0, player, 0);
  TLN_SetSpritePicture(0, 0);
  update_positions();

  /* window main loop, block until closed */
  TLN_CreateWindow(NULL, CWF_S1);
  TLN_SetMainTask(main_loop);

  /* release resources */
  TLN_DeleteWindow();
  TLN_DisableSprite(0);
  TLN_DeleteSpriteset(player);
  TLN_DeleteTilemap(tilemap);
  TLN_Deinit();
  return 0;
}
