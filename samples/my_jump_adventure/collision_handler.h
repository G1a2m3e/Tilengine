#ifndef SWEPT_AABB_H
#define SWEPT_AABB_H
#include <stdbool.h>

typedef struct {
  float x, y;
  float w, h;
  // 权重，决定碰撞后物体的推动能力，越大越容易推动对方
  float push_weight;
} HitBox;

// 保存从文件加载的精灵碰撞矩形: x_offset, y_offset为偏移量
typedef struct {
  float x_offset, y_offset;
  float w, h;
} CollisionOffset;

/*
  relative_dx:
  src和obstacle的相对x轴位移,比如如果两者都向右移动同样的距离,那么relative_dx = 0

  relative_dy:
  src和obstacle的相对y轴位移,比如如果两者都向上移动同样的距离,那么relative_dy = 0
  返回值true:发生碰撞,hit_t是碰撞发生的时间点,0表示碰撞发生在一帧的开始,1表示碰撞发生在一帧的结束
  返回值false:没有发生碰撞,hit_t是无效值
*/
extern bool swept_aabb(HitBox src, HitBox obstacle, float relative_dx, float relative_dy,
                      float *hit_t);

/*
  两个物体碰撞后的位移修正: 调用此函数前提是发生了碰撞，hit_t是碰撞发生的时间点
*/ 
extern bool resolve_displacement(HitBox src, HitBox obs, float *src_dx,
                                float *src_dy, float *obs_dx, float *obs_dy,
                                float hit_t);

/* Returns the final hitbox position in world coordinates. */
extern HitBox handle_maps_collision(HitBox src, float dx, float dy);

 /* First collision object on the tile referenced by the TMX object ID. */
extern CollisionOffset load_object_hitbox(const char *tmx_file, int object_id);

extern bool is_grounded(HitBox box);

extern bool is_touching_wall(HitBox box);

extern bool is_colliding(HitBox box1, HitBox box2);

#endif