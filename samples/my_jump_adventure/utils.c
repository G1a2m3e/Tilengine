#include "utils.h"
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
