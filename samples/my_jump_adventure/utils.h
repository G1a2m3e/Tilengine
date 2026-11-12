#ifndef UTILS_H
#define UTILS_H

#include "Tilengine.h"
#include "../../src/ObjectList.h"

/* Bind a generated sprite to its source object's position in TMX draw order. */
extern bool bind_sprite_to_object(int sprite, const TLN_Object *object);

/* Search loaded object layers by name; id == 0 means no match. */
extern TLN_ObjectInfo getObjectInfoByName(const char *name);

/* Returns the layer containing the first object with this name, or -1. */
extern int get_object_layer(const char *object_name);

/* Returns the first object layer with the exact name, or NULL if not found. */
extern TLN_ObjectList find_object_list_by_name(const char *name);

/* Returns the first bitmap background layer index, or -1 if none is loaded. */
extern int get_background_layer(void);

extern const char *get_level_file_name(int level);

typedef struct {
  float x, y;
} PathPoint;

typedef enum { PATH_PINGPONG, PATH_LOOP } PathMode;

typedef struct {
  PathPoint *points; // World coordinates of the moving object's center.
  int count;
  float speed;      // Pixels per second; default 60.
  PathMode mode;    // Default PATH_PINGPONG.
} MotionPath;

/* Initialize path to {0}; free it before loading another path into it.
   Returns false for missing/invalid references or fewer than two distinct points. */
extern bool load_object_path(int object_id, MotionPath *path);
extern void free_object_path(MotionPath *path);

#endif
