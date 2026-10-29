#include "Tilengine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Bridge loaded tilesets to sprites without parsing or reopening resources. */
#include "../src/ObjectList.h"
#include "../src/Tileset.h"

#define SCREEN_WIDTH 35*16
#define SCREEN_HEIGHT 15*16
#define PLAYER_SPEED 3
#define PLAYER_SIZE 32
#define TILE_SIZE 16
#define GRAVITY 1
#define MAX_FALL_SPEED 8

static int camera_x;
static int max_camera_x;
static int player_x;
static int player_y;
static int max_player_x;
static int velocity_y;
static TLN_Tilemap world_map;

/* Each object layer uses one tileset, so the native loader can normalize
   GIDs and convert tile-object coordinates to the top-left anchor. */
#define MAX_MAP_OBJECTS 128
static struct {
  const char *layer;
  TLN_Tileset tileset;
  TLN_Spriteset spriteset;
} object_assets[] = {
    {"player", NULL, NULL},
    {"apples", NULL, NULL},
    {"pineapples", NULL, NULL},
    {"oranges", NULL, NULL},
    {"strawberrys", NULL, NULL},
};
#define OBJECT_ASSET_COUNT (sizeof(object_assets) / sizeof(object_assets[0]))
static struct MapObject {
  TLN_ObjectInfo info;
  int asset;
} map_objects[MAX_MAP_OBJECTS];
static int num_map_objects;

/* Tile animations use one-based frame indices. Keep an empty frame 0 so
   the native TSX sequences can be used directly by sprite animations. */
static TLN_Spriteset sprites_from_tileset(TLN_Tileset tileset) {
  if (!tileset || tileset->tstype != TILESET_TILES) return NULL;
  int count = tileset->numtiles + 1;
  int width = tileset->width, height = tileset->height;
  TLN_Bitmap bitmap = TLN_CreateBitmap(width, height * count, 8);
  TLN_SpriteData *frames = calloc((size_t)count, sizeof(*frames));
  if (!bitmap || !frames) {
    if (bitmap) TLN_DeleteBitmap(bitmap);
    free(frames);
    return NULL;
  }
  TLN_Palette palette = TLN_ClonePalette(TLN_GetTilesetPalette(tileset));
  if (!palette) {
    TLN_DeleteBitmap(bitmap);
    free(frames);
    return NULL;
  }
  TLN_SetBitmapPalette(bitmap, palette);
  for (int f = 0; f < count; ++f) {
    frames[f].y = f * height;
    frames[f].w = width;
    frames[f].h = height;
    for (int y = 0; f > 0 && y < height; ++y)
      memcpy(TLN_GetBitmapPtr(bitmap, 0, f * height + y),
             tileset->data + ((f - 1) * height + y) * width, (size_t)width);
  }
  TLN_Spriteset spriteset = TLN_CreateSpriteset(bitmap, frames, count);
  free(frames);
  if (!spriteset) TLN_DeleteBitmap(bitmap);
  return spriteset;
}

static void animate_object(int sprite, int asset, int tile) {
  TLN_SequencePack pack = TLN_GetTilesetSequencePack(object_assets[asset].tileset);
  char name[16];
  snprintf(name, sizeof(name), "%d", tile);
  TLN_Sequence sequence = pack ? TLN_FindSequence(pack, name) : NULL;
  if (sequence) TLN_SetSpriteAnimation(sprite, sequence, 0);
}

static bool load_scene_objects(void) {
  for (size_t asset = 0; asset < OBJECT_ASSET_COUNT; ++asset) {
    TLN_ObjectList objects = TLN_LoadObjectList("my_jump_adventure.tmx",
                                              object_assets[asset].layer);
    if (objects == NULL) {
      fprintf(stderr, "Failed to load object layer '%s': %s\n",
              object_assets[asset].layer,
              TLN_GetErrorString(TLN_GetLastError()));
      return false;
    }
    object_assets[asset].tileset = objects->tileset;
    TLN_ObjectInfo info = {0};
    for (bool more = TLN_GetListObject(objects, &info); more;
         more = TLN_GetListObject(objects, NULL)) {
      if (num_map_objects == MAX_MAP_OBJECTS) {
        fprintf(stderr, "Too many map objects (limit: %d)\n", MAX_MAP_OBJECTS);
        TLN_DeleteObjectList(objects);
        return false;
      }
      map_objects[num_map_objects].info = info;
      map_objects[num_map_objects++].asset = (int)asset;
    }
    TLN_DeleteObjectList(objects);
  }
  return true;
}

static void free_fruits(void) {
  for (int i = 0; i < num_map_objects; ++i) TLN_DisableSprite(i + 1);
  for (size_t i = 1; i < OBJECT_ASSET_COUNT; ++i)
    if (object_assets[i].spriteset) {
      TLN_DeleteSpriteset(object_assets[i].spriteset);
      object_assets[i].spriteset = NULL;
    }
}

static bool create_fruits(void) {
  for (int i = 0; i < num_map_objects; ++i) {
    const struct MapObject *object = &map_objects[i];
    int asset = object->asset;
    if (asset <= 0 || !object->info.visible) continue;
    if (!object_assets[asset].spriteset) {
      object_assets[asset].spriteset = sprites_from_tileset(object_assets[asset].tileset);
      if (!object_assets[asset].spriteset) return false;
    }
    int frame = object->info.gid + 1;
    if (!TLN_ConfigSprite(i + 1, object_assets[asset].spriteset, 0) ||
        !TLN_SetSpritePicture(i + 1, frame)) return false;
    animate_object(i + 1, asset, object->info.gid);
    TLN_SetSpriteFlags(i + 1, object->info.flags);
    TLN_SetSpriteScaling(i + 1, (float)object->info.width / TLN_GetTileWidth(object_assets[asset].tileset),
                         (float)object->info.height / TLN_GetTileHeight(object_assets[asset].tileset));
  }
  return true;
}



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
      if (TLN_GetTilemapTile(world_map, row, col, &tile) && tile.index != 0)
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
  for (int i = 0; i < num_map_objects; ++i)
    if (map_objects[i].asset > 0 && map_objects[i].info.visible)
      TLN_SetSpritePosition(i + 1, map_objects[i].info.x - camera_x,
                            map_objects[i].info.y);
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
  /* Reserve sprite 0 for the player and one slot per possible map object. */
  TLN_Init(SCREEN_WIDTH, SCREEN_HEIGHT, 2, MAX_MAP_OBJECTS + 1, 0);
  TLN_SetBGColor(32, 32, 128);

  /* Resolve TMX, TSX and bitmap resources from the same directory. */
  TLN_SetLoadPath("assets/my_jump_adventure");
  if (!load_scene_objects()) {
    TLN_Deinit();
    return 1;
  }
  TLN_Tilemap tilemap = TLN_LoadTilemap("my_jump_adventure.tmx", "maps");
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

  bool found_player = false;
  for (int i = 0; i < num_map_objects; ++i) {
    if (map_objects[i].asset == 0 && map_objects[i].info.visible) {
      player_x = map_objects[i].info.x;
      /* The native loader has already converted the bottom-left anchor. */
      player_y = map_objects[i].info.y;
      found_player = true;
      break;
    }
  }
  if (!found_player) {
    fprintf(stderr, "No player tile object found in the map\n");
    TLN_DeleteTilemap(tilemap);
    TLN_Deinit();
    return 1;
  }

  TLN_Spriteset player = sprites_from_tileset(object_assets[0].tileset);
  if (player == NULL) {
    fprintf(stderr, "Failed to create player spriteset\n");
    TLN_DeleteTilemap(tilemap);
    TLN_Deinit();
    return 1;
  }
  TLN_ConfigSprite(0, player, 0);
  TLN_SetSpritePicture(0, 1);
  animate_object(0, 0, 0);
  /* Layer 1 is behind terrain layer 0. Bitmap layers repeat on both axes,
     matching the current TMX image layer's repeatx/repeaty settings. */
  TLN_Bitmap background = TLN_LoadBitmap("Background/Purple.png");
  if (background == NULL || !TLN_SetLayerBitmap(1, background)) {
    fprintf(stderr, "Failed to load background: %s\n",
            TLN_GetErrorString(TLN_GetLastError()));
    if (background != NULL)
      TLN_DeleteBitmap(background);
    TLN_DisableSprite(0);
    TLN_DeleteSpriteset(player);
    TLN_DeleteTilemap(tilemap);
    TLN_Deinit();
    return 1;
  }
  TLN_SetLayerPosition(1, 0, 0);

  if (!create_fruits()) {
    fprintf(stderr, "Failed to create fruit sprites: %s\n",
            TLN_GetErrorString(TLN_GetLastError()));
    free_fruits();
    TLN_DisableSprite(0);
    TLN_DeleteSpriteset(player);
    TLN_DisableLayer(1);
    TLN_DeleteBitmap(background);
    TLN_DeleteTilemap(tilemap);
    TLN_Deinit();
    return 1;
  }
  update_positions();

  /* window main loop, block until closed */
  TLN_CreateWindow(NULL, CWF_FULLSCREEN);
  TLN_SetMainTask(main_loop);

  /* release resources */
  TLN_DeleteWindow();
  free_fruits();
  TLN_DisableSprite(0);
  TLN_DeleteSpriteset(player);
  TLN_DisableLayer(1);
  TLN_DeleteBitmap(background);
  TLN_DeleteTilemap(tilemap);
  TLN_Deinit();
  return 0;
}
