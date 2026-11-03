
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

bool ResolveDisplacement(Rect src, Rect obs, float *src_dx, float *src_dy,
                         float *obs_dx, float *obs_dy, float hit_t) {
  // 1. 相对位移
  float rdx = *src_dx - *obs_dx;
  float rdy = *src_dy - *obs_dy;

  // 2. 计算各轴的碰撞进入时间
  float tx = -INFINITY;
  float ty = -INFINITY;

  /**
    src向右靠近obs，碰撞发生在src的右边界与obs的左边界接触时
    src.x + src.w + rdx * t = obs.x
    t = (obs.x - src.x - src.w) / rdx
    所以t的含义是一帧进行到多少比例时会发生碰撞，0表示碰撞发生在一帧的开始，1表示碰撞发生在一帧的结束
  */
  if (rdx > 0.0f)
    tx = (obs.x - src.x - src.w) / rdx;
  /**
    src向左靠近obs， 碰撞发生在src的左边界与obs的右边界接触时
    src.x + rdx * t = obs.x + obs.w
    t = (obs.x + obs.w - src.x) / rdx
  */
  else if (rdx < 0.0f)
    tx = (obs.x + obs.w - src.x) / rdx;

  /**
    y的计算同理
  */
  if (rdy > 0.0f)
    ty = (obs.y - src.y - src.h) / rdy;
  else if (rdy < 0.0f)
    ty = (obs.y + obs.h - src.y) / rdy;

  /**
    3. 确定碰撞轴和运动方向
    但是，仅仅在X方向接触，并不代表两个二维矩形已经发生碰撞。
    因为两个矩形要真正碰撞，必须同时满足X方向的投影发生重叠和Y方向的投影发生重叠
    所以首次碰撞时间是max(tx, ty)，也就是两个轴中进入时间较晚的那个时间点
    如果X轴进入时间更晚，说明最后造成两个矩形接触的是X方向，碰撞法线沿X轴也就是x_axis = true
  */
  bool x_axis = (tx >= ty);

  /**
    如果属于X方向碰撞，后续修正双方的dx                        
    如果属于Y方向碰撞，后续修正双方的dy
    例如角色斜向右下运动，撞到墙壁的左侧，那么只需要修正dx，dy可以保留，角色就能沿墙壁向下滑动
  */
  float *a = x_axis ? src_dx : src_dy;
  float *b = x_axis ? obs_dx : obs_dy;

  float relative = x_axis ? rdx : rdy;

  if (relative == 0.0f)
    return false;

  // 将正方向统一为 src 朝 obs 靠近的方向
  float direction = relative > 0.0f ? 1.0f : -1.0f;

  float va = *a * direction;
  float vb = *b * direction;

  if (va <= vb)
    return false;

  // 4. 获取推动权重
  float wa = src.push_weight;
  float wb = obs.push_weight;

  // 权重必须大于 0，可以使用 INFINITY
  if (!(wa > 0.0f) || !(wb > 0.0f))
    return false;

  if (isinf(wa) && isinf(wb))
    return false;

  // 5. 根据权重计算接触后的共同运动速度
  float v;

  if (isinf(wa)) {
    v = va;
  } else if (isinf(wb)) {
    v = vb;
  } else if (va >= 0.0f && vb >= 0.0f) {
    // src 从后面追上 obs
    float k = fminf(1.0f, wa / wb);
    v = vb + (va - vb) * k;
  } else if (va <= 0.0f && vb <= 0.0f) {
    // obs 从后面追上 src
    float k = fminf(1.0f, wb / wa);
    v = va + (vb - va) * k;
  } else {
    // 两个物体相向运动
    float balance = wa * va + wb * vb;

    if (balance >= 0.0f)
      v = fminf(va, balance / wb);
    else
      v = fmaxf(vb, balance / wa);
  }

  // 6. 仅修正碰撞之后的剩余位移
  float remaining = 1.0f - hit_t;

  *a += (v - va) * remaining * direction;
  *b += (v - vb) * remaining * direction;

  return true;
}