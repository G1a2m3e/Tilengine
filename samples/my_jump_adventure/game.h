#ifndef GAME_H
#define GAME_H

#include <stdint.h>
extern void init_game();
extern void update(uint32_t frame);
extern void render(uint32_t frame);
#endif