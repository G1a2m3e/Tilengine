#ifndef SWEPT_AABB_H
#define SWEPT_AABB_H
#include <stdbool.h>

typedef struct {
  float x, y;
  float w, h;
} Rect;

bool SweptAABB(Rect src, Rect obstacle, float dx, float dy, float *hit_t);
#endif