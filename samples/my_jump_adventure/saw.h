#ifndef SAW_H
#define SAW_H

#include "traps.h"
G_BEGIN_DECLS

#define T_TYPE_SAW (t_saw_get_type())

G_DECLARE_FINAL_TYPE(TSaw, t_saw, T, SAW, TTrap)

G_END_DECLS

#endif