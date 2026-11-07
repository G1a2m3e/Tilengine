#include "utils.h"
#include "../../src/ObjectList.h"
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

int get_object_layer(const char *object_name) {
  if (!object_name || !object_name[0] || !TLN_GetContext())
    return -1;

  for (int layer = 0; layer < TLN_GetNumLayers(); ++layer) {
    if (TLN_GetLayerType(layer) != LAYER_OBJECT)
      continue;
    TLN_ObjectList list = TLN_GetLayerObjects(layer);
    if (!list)
      continue;
    for (TLN_Object *obj = list->list; obj; obj = obj->next) {
      if (strncmp(obj->name, object_name, sizeof(obj->name)) == 0)
        return layer;
    }
  }
  return -1;
}

TLN_ObjectList find_object_list(const char *name) {
  if (!name || !name[0] || !TLN_GetContext())
    return NULL;

  for (int i = 0; i < TLN_GetNumLayers(); ++i) {
    TLN_ObjectList list = TLN_GetLayerObjects(i);
    if (list && strcmp(list->name, name) == 0)
      return list;
  }
  return NULL;
}

int get_background_layer(void) {
  if (!TLN_GetContext())
    return -1;

  for (int layer = 0; layer < TLN_GetNumLayers(); ++layer) {
    if (TLN_GetLayerType(layer) == LAYER_BITMAP && TLN_GetLayerBitmap(layer))
      return layer;
  }
  return -1;
}

const char *get_level_file_name(int level) {
  static char buffer[32];
  snprintf(buffer, sizeof(buffer), "level%d.tmx", level);
  return buffer;
}
