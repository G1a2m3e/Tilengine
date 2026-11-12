#include "utils.h"
#include "../../src/LoadFile.h"
#include "../../src/ObjectList.h"
#include "game.h"
#include <glib.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

bool bind_sprite_to_object(int sprite, const TLN_Object *object) {
  if (!object || !TLN_GetContext())
    return false;

  for (int layer = 0; layer < TLN_GetNumLayers(); ++layer) {
    TLN_ObjectList objects = TLN_GetLayerObjects(layer);
    if (!objects)
      continue;
    for (TLN_Object *item = objects->list; item; item = item->next)
      if (item == object)
        return TLN_SetSpriteObject(sprite, layer, object->id);
  }
  return false;
}

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

TLN_ObjectList find_object_list_by_name(const char *name) {
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

// First pass reads the object's properties; second pass resolves its path ID.
typedef struct {
  int object_id, path_id, pass;
  bool selected, valid;
  float speed, rotation;
  PathMode mode;
  PathPoint origin, offset;
  GArray *points, *offsets;
} PathLoader;

static const char *path_attribute(const char **names, const char **values,
                                  const char *name) {
  for (int i = 0; names[i]; ++i)
    if (strcmp(names[i], name) == 0)
      return values[i];
  return NULL;
}

static void path_start(GMarkupParseContext *context, const char *tag,
                       const char **names, const char **values,
                       gpointer user_data, GError **error) {
  PathLoader *loader = user_data;
  const char *x = path_attribute(names, values, "x");
  const char *y = path_attribute(names, values, "y");
  if (!strcmp(tag, "group") || !strcmp(tag, "objectgroup")) {
    g_array_append_val(loader->offsets, loader->offset);
    const char *ox = path_attribute(names, values, "offsetx");
    const char *oy = path_attribute(names, values, "offsety");
    if (ox)
      loader->offset.x += g_ascii_strtod(ox, NULL);
    if (oy)
      loader->offset.y += g_ascii_strtod(oy, NULL);
  } else if (!strcmp(tag, "object")) {
    const char *id = path_attribute(names, values, "id");
    int target = loader->pass == 0 ? loader->object_id : loader->path_id;
    loader->selected = id && g_ascii_strtoll(id, NULL, 10) == target;
    loader->origin = loader->offset;
    if (x)
      loader->origin.x += g_ascii_strtod(x, NULL);
    if (y)
      loader->origin.y += g_ascii_strtod(y, NULL);
    const char *rotation = path_attribute(names, values, "rotation");
    loader->rotation =
        rotation ? g_ascii_strtod(rotation, NULL) * G_PI / 180 : 0;
  } else if (loader->selected && loader->pass == 0 &&
             !strcmp(tag, "property")) {
    const char *name = path_attribute(names, values, "name");
    const char *value = path_attribute(names, values, "value");
    if (!name || !value)
      return;
    if (!strcmp(name, "path"))
      loader->path_id = g_ascii_strtoll(value, NULL, 10);
    else if (!strcmp(name, "speed"))
      loader->speed = g_ascii_strtod(value, NULL);
    else if (!strcmp(name, "move_mode")) {
      if (!strcmp(value, "loop"))
        loader->mode = PATH_LOOP;
      else if (!strcmp(value, "pingpong"))
        loader->mode = PATH_PINGPONG;
      else
        loader->valid = false;
    }
  } else if (loader->selected && loader->pass == 1 &&
             !strcmp(tag, "polyline")) {
    const char *text = path_attribute(names, values, "points");
    if (!text)
      return;
    while (*text) {
      while (g_ascii_isspace(*text))
        ++text;
      if (!*text)
        break;
      char *end;
      double px = g_ascii_strtod(text, &end);
      if (end == text || *end != ',') {
        loader->valid = false;
        break;
      }
      text = end + 1;
      double py = g_ascii_strtod(text, &end);
      if (end == text || (*end && !g_ascii_isspace(*end))) {
        loader->valid = false;
        break;
      }
      text = end;
      PathPoint point = {loader->origin.x + px * cosf(loader->rotation) -
                             py * sinf(loader->rotation),
                         loader->origin.y + px * sinf(loader->rotation) +
                             py * cosf(loader->rotation)};
      if (!isfinite(point.x) || !isfinite(point.y)) {
        loader->valid = false;
        break;
      }
      if (loader->points->len) {
        PathPoint last =
            g_array_index(loader->points, PathPoint, loader->points->len - 1);
        if (point.x == last.x && point.y == last.y)
          continue;
      }
      g_array_append_val(loader->points, point);
    }
  }
}

static void path_end(GMarkupParseContext *context, const char *tag,
                     gpointer user_data, GError **error) {
  PathLoader *loader = user_data;
  if (!strcmp(tag, "object"))
    loader->selected = false;
  if (!strcmp(tag, "group") || !strcmp(tag, "objectgroup")) {
    loader->offset =
        g_array_index(loader->offsets, PathPoint, loader->offsets->len - 1);
    g_array_set_size(loader->offsets, loader->offsets->len - 1);
  }
}

bool load_object_path(int object_id, MotionPath *path) {
  const char *tmx_file = get_level_file_name(get_current_level());
  ssize_t size;
  char *xml = LoadFile(tmx_file, &size);
  if (!xml)
    return false;
  PathLoader loader = {.object_id = object_id,
                       .speed = 60,
                       .valid = true,
                       .points = g_array_new(FALSE, FALSE, sizeof(PathPoint)),
                       .offsets = g_array_new(FALSE, FALSE, sizeof(PathPoint))};
  GMarkupParser callbacks = {.start_element = path_start,
                             .end_element = path_end};
  for (loader.pass = 0; loader.pass < 2 && loader.valid; ++loader.pass) {
    loader.selected = false;
    loader.offset = (PathPoint){0};
    GMarkupParseContext *parser =
        g_markup_parse_context_new(&callbacks, 0, &loader, NULL);
    loader.valid = g_markup_parse_context_parse(parser, xml, size, NULL) &&
                   g_markup_parse_context_end_parse(parser, NULL) &&
                   loader.valid;
    g_markup_parse_context_free(parser);
    if (loader.path_id <= 0)
      loader.valid = false;
  }
  free(xml);
  g_array_free(loader.offsets, TRUE);

  if (loader.mode == PATH_LOOP && loader.points->len > 1) {
    PathPoint first = g_array_index(loader.points, PathPoint, 0);
    PathPoint last =
        g_array_index(loader.points, PathPoint, loader.points->len - 1);
    if (first.x == last.x && first.y == last.y)
      g_array_set_size(loader.points, loader.points->len - 1);
  }
  if (!loader.valid || loader.points->len < 2 || !isfinite(loader.speed) ||
      loader.speed <= 0) {
    g_array_free(loader.points, TRUE);
    return false;
  }
  *path = (MotionPath){
      .count = loader.points->len, .speed = loader.speed, .mode = loader.mode};
  path->points = (PathPoint *)g_array_free(loader.points, FALSE);
  return true;
}

void free_object_path(MotionPath *path) {
  g_free(path->points);
  *path = (MotionPath){0};
}
