#ifndef FALLING_PLATFORM_H
#define FALLING_PLATFORM_H

#include "traps.h"

G_BEGIN_DECLS

#define T_TYPE_FALLING_PLATFORM (t_falling_platform_get_type())

G_DECLARE_FINAL_TYPE(TFallingPlatform, t_falling_platform, T, FALLING_PLATFORM,
                     TTrap)

/* Idempotent: leaving or touching the platform again does not restart its timer. */
void t_falling_platform_touch(TFallingPlatform *self);
gboolean t_falling_platform_is_offscreen(TFallingPlatform *self,
                                        int viewport_bottom);

G_END_DECLS

#endif
