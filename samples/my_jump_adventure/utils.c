#include "utils.h"
#include "../../src/LoadFile.h"
#include "../../src/LoadTMX.h"
#include "../../src/simplexml.h"
#include <stdlib.h>
#include <string.h>

TLN_ObjectInfo getObjectInfoByName(const char *name) {
  TLN_ObjectInfo not_found = {0};
  if (!name || !name[0] || !TLN_GetContext())
    return not_found;

  for (int layer = 0; layer < TLN_GetNumLayers(); ++layer) {
    TLN_ObjectList objects = TLN_GetLayerObjects(layer);
    if (!objects)
      continue;

    TLN_ObjectInfo object = {0};
    for (bool more = TLN_GetListObject(objects, &object); more;
         more = TLN_GetListObject(objects, NULL)) {
      if (strncmp(object.name, name, sizeof(object.name)) == 0)
        return object;
    }
  }
  return not_found;
}

static void parse_xml(const char *filename, SimpleXmlTagHandler handler) {
  ssize_t size;
  char *xml = LoadFile(filename, &size);
  if (!xml) return;
  SimpleXmlParser parser = simpleXmlCreateParser(xml, size);
  if (parser) {
    simpleXmlParse(parser, handler);
    simpleXmlDestroyParser(parser);
  }
  free(xml);
}

static struct {
  Hitbox box;
  int object_id, current_object;
  uint32_t gid, current_gid;
  int target_tile, current_tile;
  bool in_group, found;
} hitbox_loader;

static void *read_object_gid(SimpleXmlParser parser, SimpleXmlEvent event,
                             const char *tag, const char *attribute,
                             const char *value) {
  (void)parser;
  if (strcmp(tag, "object") != 0) return read_object_gid;
  if (event == ADD_SUBTAG) {
    hitbox_loader.current_object = 0;
    hitbox_loader.current_gid = 0;
  } else if (event == ADD_ATTRIBUTE) {
    if (strcmp(attribute, "id") == 0)
      hitbox_loader.current_object = atoi(value);
    else if (strcmp(attribute, "gid") == 0)
      hitbox_loader.current_gid = (uint32_t)strtoul(value, NULL, 10);
  } else if (event == FINISH_ATTRIBUTES &&
             hitbox_loader.current_object == hitbox_loader.object_id)
    hitbox_loader.gid = hitbox_loader.current_gid & 0x0FFFFFFFu;
  return read_object_gid;
}

/* Read only the first collision object belonging to the referenced tile. */
static void *read_hitbox(SimpleXmlParser parser, SimpleXmlEvent event,
                         const char *tag, const char *attribute,
                         const char *value) {
  (void)parser;
  if (event == ADD_SUBTAG && strcmp(tag, "objectgroup") == 0)
    hitbox_loader.in_group = hitbox_loader.current_tile == hitbox_loader.target_tile;
  else if (event == ADD_ATTRIBUTE) {
    if (strcmp(tag, "tile") == 0 && strcmp(attribute, "id") == 0)
      hitbox_loader.current_tile = atoi(value);
    else if (strcmp(tag, "object") == 0 && hitbox_loader.in_group &&
             !hitbox_loader.found) {
      float number = strtof(value, NULL);
      if (strcmp(attribute, "x") == 0)
        hitbox_loader.box.x = number;
      else if (strcmp(attribute, "y") == 0)
        hitbox_loader.box.y = number;
      else if (strcmp(attribute, "width") == 0)
        hitbox_loader.box.width = number;
      else if (strcmp(attribute, "height") == 0)
        hitbox_loader.box.height = number;
    }
  } else if (event == FINISH_ATTRIBUTES && strcmp(tag, "object") == 0 &&
             hitbox_loader.in_group)
    hitbox_loader.found = true;
  else if (event == FINISH_TAG) {
    if (strcmp(tag, "objectgroup") == 0)
      hitbox_loader.in_group = false;
    else if (strcmp(tag, "tile") == 0)
      hitbox_loader.current_tile = -1;
  }
  return read_hitbox;
}

Hitbox load_object_hitbox(const char *tmx_file, int object_id) {
  memset(&hitbox_loader, 0, sizeof(hitbox_loader));
  hitbox_loader.object_id = object_id;
  hitbox_loader.current_tile = -1;

  TMXInfo map = {0};
  TMXLoad(tmx_file, &map);
  parse_xml(tmx_file, read_object_gid);
  if (!hitbox_loader.gid) return hitbox_loader.box;

  /* The largest firstgid <= GID identifies its tileset. */
  TMXTileset *tileset = NULL;
  for (int i = 0; i < map.num_tilesets; ++i)
    if (hitbox_loader.gid >= (uint32_t)map.tilesets[i].firstgid &&
        (!tileset || map.tilesets[i].firstgid > tileset->firstgid))
      tileset = &map.tilesets[i];
  if (!tileset) return hitbox_loader.box;

  hitbox_loader.target_tile = hitbox_loader.gid - tileset->firstgid;
  FileInfo path;
  char filename[300];
  SplitFilename(tmx_file, &path);
  BuildFilePath(filename, sizeof(filename), path.path, tileset->source, NULL);
  parse_xml(filename, read_hitbox);
  return hitbox_loader.box;
}
