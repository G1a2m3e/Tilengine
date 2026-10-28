#include "Tilengine.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/simplexml.h"

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

/* TMX object groups may mix tilesets. Read original GIDs here because
   TLN_LoadObjectList normalizes all objects against one tileset. */
#define MAX_MAP_OBJECTS 128
static struct {
  const char *tsx;
  const char *image;
  uint32_t firstgid;
  TLN_Spriteset spriteset;
} object_assets[] = {
    {"player_Idle.tsx", "player_Idle.png", 0, NULL},
    {"Apple.tsx", "Fruits/Apple.png", 0, NULL},
    {"Pineapple.tsx", "Fruits/Pineapple.png", 0, NULL},
    {"Orange.tsx", "Fruits/Orange.png", 0, NULL},
    {"Strawberry.tsx", "Fruits/Strawberry.png", 0, NULL},
};
#define OBJECT_ASSET_COUNT (sizeof(object_assets) / sizeof(object_assets[0]))
static struct MapObject {
  uint32_t gid;
  float x, y, width, height;
  bool visible;
} map_objects[MAX_MAP_OBJECTS], pending_object;
static int num_map_objects;
static bool object_overflow;
static uint32_t pending_firstgid;
static char pending_tileset_source[256];

static void *read_scene_xml(SimpleXmlParser parser, SimpleXmlEvent event,
                            const char *tag, const char *attribute,
                            const char *value) {
  (void)parser;
  if (event == ADD_SUBTAG) {
    if (strcmp(tag, "tileset") == 0) {
      pending_firstgid = 0;
      pending_tileset_source[0] = '\0';
    }
    if (strcmp(tag, "object") == 0) {
      memset(&pending_object, 0, sizeof(pending_object));
      pending_object.visible = true;
    }
  } else if (event == ADD_ATTRIBUTE) {
    if (strcmp(tag, "tileset") == 0) {
      if (strcmp(attribute, "firstgid") == 0)
        pending_firstgid = (uint32_t)strtoul(value, NULL, 10);
      else if (strcmp(attribute, "source") == 0)
        snprintf(pending_tileset_source, sizeof(pending_tileset_source), "%s", value);
    } else if (strcmp(tag, "object") == 0) {
      if (strcmp(attribute, "gid") == 0)
        pending_object.gid = (uint32_t)strtoul(value, NULL, 10);
      else if (strcmp(attribute, "x") == 0) pending_object.x = strtof(value, NULL);
      else if (strcmp(attribute, "y") == 0) pending_object.y = strtof(value, NULL);
      else if (strcmp(attribute, "width") == 0) pending_object.width = strtof(value, NULL);
      else if (strcmp(attribute, "height") == 0) pending_object.height = strtof(value, NULL);
      else if (strcmp(attribute, "visible") == 0) pending_object.visible = atoi(value) != 0;
    }
  } else if (event == FINISH_ATTRIBUTES && strcmp(tag, "tileset") == 0) {
    for (size_t i = 0; i < OBJECT_ASSET_COUNT; ++i)
      if (strcmp(pending_tileset_source, object_assets[i].tsx) == 0)
        object_assets[i].firstgid = pending_firstgid;
  } else if (event == FINISH_TAG && strcmp(tag, "object") == 0 && pending_object.gid) {
    if (num_map_objects < MAX_MAP_OBJECTS)
      map_objects[num_map_objects++] = pending_object;
    else
      object_overflow = true;
  }
  return read_scene_xml;
}

static bool load_scene_objects(void) {
  FILE *file = fopen("assets/my_jump_adventure/my_jump_adventure.tmx", "rb");
  if (!file) return false;
  if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return false; }
  long size = ftell(file);
  if (size <= 0 || fseek(file, 0, SEEK_SET) != 0) { fclose(file); return false; }
  char *xml = malloc((size_t)size);
  if (!xml) { fclose(file); return false; }
  bool loaded = fread(xml, 1, (size_t)size, file) == (size_t)size;
  fclose(file);
  SimpleXmlParser parser = loaded ? simpleXmlCreateParser(xml, size) : NULL;
  bool ok = parser && simpleXmlParse(parser, read_scene_xml) == 0;
  if (parser) simpleXmlDestroyParser(parser);
  free(xml);
  return ok && !object_overflow;
}

static int object_asset(const struct MapObject *object) {
  uint32_t gid = object->gid & 0x0fffffff;
  int best = -1;
  for (size_t i = 0; i < OBJECT_ASSET_COUNT; ++i)
    if (object_assets[i].firstgid && gid >= object_assets[i].firstgid &&
        (best < 0 || object_assets[i].firstgid > object_assets[best].firstgid))
      best = (int)i;
  return best;
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
    int asset = object_asset(object);
    if (asset <= 0 || !object->visible) continue;
    if (!object_assets[asset].spriteset) {
      TLN_Bitmap bitmap = TLN_LoadBitmap(object_assets[asset].image);
      if (!bitmap) return false;
      int cols = TLN_GetBitmapWidth(bitmap) / PLAYER_SIZE;
      int count = cols * (TLN_GetBitmapHeight(bitmap) / PLAYER_SIZE);
      if (count <= 0) { TLN_DeleteBitmap(bitmap); return false; }
      TLN_SpriteData *frames = calloc((size_t)count, sizeof(*frames));
      if (!frames) { TLN_DeleteBitmap(bitmap); return false; }
      for (int f = 0; f < count; ++f) {
        frames[f].x = (f % cols) * PLAYER_SIZE;
        frames[f].y = (f / cols) * PLAYER_SIZE;
        frames[f].w = frames[f].h = PLAYER_SIZE;
      }
      object_assets[asset].spriteset = TLN_CreateSpriteset(bitmap, frames, count);
      free(frames);
      if (!object_assets[asset].spriteset) { TLN_DeleteBitmap(bitmap); return false; }
    }
    int frame = (int)((object->gid & 0x0fffffff) - object_assets[asset].firstgid);
    if (!TLN_ConfigSprite(i + 1, object_assets[asset].spriteset, 0) ||
        !TLN_SetSpritePicture(i + 1, frame)) return false;
    uint32_t flags = 0;
    if (object->gid & 0x80000000) flags |= FLAG_FLIPX;
    if (object->gid & 0x40000000) flags |= FLAG_FLIPY;
    TLN_SetSpriteFlags(i + 1, flags);
    TLN_SetSpriteScaling(i + 1, object->width / PLAYER_SIZE, object->height / PLAYER_SIZE);
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
    if (object_asset(&map_objects[i]) > 0 && map_objects[i].visible)
      TLN_SetSpritePosition(i + 1, (int)map_objects[i].x - camera_x,
                            (int)(map_objects[i].y - map_objects[i].height));
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
  if (!load_scene_objects()) {
    fprintf(stderr, "Failed to read map objects (limit: %d)\n", MAX_MAP_OBJECTS);
    return 1;
  }
  /* Reserve sprite 0 for the player and one slot per map object. */
  TLN_Init(SCREEN_WIDTH, SCREEN_HEIGHT, 2, num_map_objects + 1, 0);
  TLN_SetBGColor(32, 32, 128);

  /* Resolve TMX, TSX and bitmap resources from the same directory. */
  TLN_SetLoadPath("assets/my_jump_adventure");
  TLN_Tilemap tilemap = TLN_LoadTilemap("my_jump_adventure.tmx", NULL);
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
    if (object_asset(&map_objects[i]) == 0) {
      player_x = (int)map_objects[i].x;
      /* Tiled tile objects use a bottom-left anchor. */
      player_y = (int)(map_objects[i].y - map_objects[i].height);
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

  /* The first idle frame occupies the first 32x32 pixels of the sheet. */
  TLN_Bitmap bitmap = TLN_LoadBitmap("player_Idle.png");
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
