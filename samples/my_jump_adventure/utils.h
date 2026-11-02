#ifndef UTILS_H
#define UTILS_H

#include "Tilengine.h"

typedef struct {
  float x, y;
  float width, height;
} Hitbox;

/* Search loaded object layers by name; id == 0 means no match. */
TLN_ObjectInfo getObjectInfoByName(const char *name);

/* First collision object on the tile referenced by the TMX object ID. */
Hitbox load_object_hitbox(const char *tmx_file, int object_id);
#endif