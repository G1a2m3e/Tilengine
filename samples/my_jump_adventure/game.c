#include "game.h"
#include "player.h"
#include "Tilengine.h"

static Player player;
static uint32_t last_update_ms;

void init_game() {
  last_update_ms = TLN_GetTicks();
  init_player(&player, "virtual_guy");
}

void update(uint32_t frame) { 
  uint32_t now = TLN_GetTicks();
  uint32_t elapsed = now - last_update_ms;
  last_update_ms = now;
  update_player(&player, elapsed); 
}

void render(uint32_t frame) { render_player(&player, frame); }