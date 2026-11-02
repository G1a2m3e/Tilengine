
#include "swept_aabb.h"
#include <math.h>
#include <stdbool.h>

bool SweptAABB(Rect src, Rect obstacle, float dx, float dy, float *hit_t) {
  // 初始已经重叠
  if (src.x < obstacle.x + obstacle.w && src.x + src.w > obstacle.x &&
      src.y < obstacle.y + obstacle.h && src.y + src.h > obstacle.y) {
    *hit_t = 0.0f;
    return true;
  }

  // Minkowski 扩张：将移动矩形转换为左上角的点
  float left = obstacle.x - src.w;
  float right = obstacle.x + obstacle.w;
  float top = obstacle.y - src.h;
  float bottom = obstacle.y + obstacle.h;

  float tx_entry, tx_exit;
  float ty_entry, ty_exit;

  // X 轴进入和离开时间
  if (dx == 0.0f) {
    if (src.x <= left || src.x >= right)
      return false;

    tx_entry = -INFINITY;
    tx_exit = INFINITY;
  } else {
    float t1 = (left - src.x) / dx;
    float t2 = (right - src.x) / dx;

    tx_entry = fminf(t1, t2);
    tx_exit = fmaxf(t1, t2);
  }

  // Y 轴进入和离开时间
  if (dy == 0.0f) {
    if (src.y <= top || src.y >= bottom)
      return false;

    ty_entry = -INFINITY;
    ty_exit = INFINITY;
  } else {
    float t1 = (top - src.y) / dy;
    float t2 = (bottom - src.y) / dy;

    ty_entry = fminf(t1, t2);
    ty_exit = fmaxf(t1, t2);
  }

  // 两个轴的时间区间求交集
  float entry = fmaxf(tx_entry, ty_entry);
  float exit = fminf(tx_exit, ty_exit);

  if (entry > exit || exit < 0.0f || entry < 0.0f || entry > 1.0f) {
    return false;
  }

  *hit_t = entry;
  return true;
}
