#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  const char *player_name;
  const char *fruits_objectgroup_name;
} LevelInfo;

extern void init_game(int current_level, LevelInfo level_info);
extern void reset_game_timer(void);
extern bool update(uint32_t frame);
extern void render(uint32_t frame);
extern void load_level(int level);
extern int get_current_level();
#endif
