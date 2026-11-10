#ifndef FALLING_PLATFORM_H
#define FALLING_PLATFORM_H

#include "traps.h"

G_BEGIN_DECLS

#define T_TYPE_FALLING_PLATFORM (t_falling_platform_get_type())

G_DECLARE_FINAL_TYPE(TFallingPlatform, t_falling_platform, T, FALLING_PLATFORM,
                     TTrap)

/* Idempotent: leaving or touching the platform again does not restart its timer. */
void t_falling_platform_touch(TFallingPlatform *self);

/* 角色是否站在当前平台顶面。 */
gboolean t_falling_platform_is_supporting(TFallingPlatform *self, HitBox box);

/* 根据平台本帧位移修正站立角色的期望位移，成功时触发掉落计时。 */
gboolean t_falling_platform_carry(TFallingPlatform *self, HitBox box,
                                 float *dx, float *dy);

/* 只查询顶面碰撞，输出碰撞时间和落点；选定最终落点后再触发计时。 */
gboolean t_falling_platform_check_collision(TFallingPlatform *self, HitBox src,
                                           float dx, float dy, float *hit_t,
                                           HitBox *result);

gboolean t_falling_platform_is_offscreen(TFallingPlatform *self,
                                        int viewport_bottom);

G_END_DECLS

#endif
