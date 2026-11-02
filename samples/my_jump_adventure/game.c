#include "game.h"
#include "player.h"

static Player player;

void init_game() {
  init_player(&player, "virtual_guy",
              "Main Characters/Virtual Guy/Idle (32x32).png");
}

void update(uint32_t frame) { update_player(&player, frame); }

void render(uint32_t frame) { render_player(&player, frame); }