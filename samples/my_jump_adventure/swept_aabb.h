#ifndef SWEPT_AABB_H
#define SWEPT_AABB_H
#include <stdbool.h>

typedef struct {
  float x, y;
  float w, h;
  // 权重，决定碰撞后物体的推动能力，越大越容易推动对方
  float push_weight;
} Rect;

extern bool SweptAABB(Rect src, Rect obstacle, float dx, float dy,
                      float *hit_t);

/*
  两个物体碰撞后的位移修正: 调用此函数前提是发生了碰撞，hit_t 是碰撞发生的时间点
*/ 
extern bool ResolveDisplacement(Rect src, Rect obs, float *src_dx,
                                float *src_dy, float *obs_dx, float *obs_dy,
                                float hit_t);
#endif