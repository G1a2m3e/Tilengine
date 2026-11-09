#ifndef FALLING_PLATFORM_H
#define FALLING_PLATFORM_H

#include "traps.h"

G_BEGIN_DECLS

#define T_TYPE_FALLING_PLATFORM (t_falling_platform_get_type())

G_DECLARE_FINAL_TYPE(TFallingPlatform, t_falling_platform, T, FALLING_PLATFORM,
                     TTrap)

G_END_DECLS

#endif