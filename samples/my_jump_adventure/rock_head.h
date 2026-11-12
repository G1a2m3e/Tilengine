#ifndef ROCK_HEAD_H
#define ROCK_HEAD_H
#include "traps.h"

G_BEGIN_DECLS

#define T_TYPE_ROCKHEAD (t_rockhead_get_type())

G_DECLARE_FINAL_TYPE(TRockhead, t_rockhead, T, ROCKHEAD, TTrap)

G_END_DECLS

#endif