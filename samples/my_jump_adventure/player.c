#include "player.h"
#include "Tilengine.h"
#include "utils.h"

void init_player(Player *player, const char *name, const char *spriteset_name) {
  TLN_ObjectInfo obj = getObjectInfoByName(name);
  TLN_Spriteset set = TLN_LoadSpriteset(spriteset_name);
  TLN_Sequence idle = TLN_CreateSpriteSequence("idle", set, "idle", 3);
  TLN_SetSpriteSet(PLAYER_SPRITES, set);
  player->x = obj.x;
  player->y = obj.y;
  TLN_SetSpritePosition(PLAYER_SPRITES, player->x, player->y);
  TLN_SetSpriteAnimation(PLAYER_SPRITES, idle, 0);
  TLN_DisableLayer(0);
  player->facing_right = true;
  player->state = IDLE;
}
void player_move_to_right(Player *player) {
  player->facing_right = true;
  player->x += 2;
  TLN_SetSpriteFlags(PLAYER_SPRITES, 0);
  TLN_SetSpritePosition(0, player->x, player->y);
}
void player_move_to_left(Player *player) {
  player->facing_right = false;
  player->x -= 2;
  TLN_SetSpriteFlags(PLAYER_SPRITES, FLAG_FLIPX);
  TLN_SetSpritePosition(0, player->x, player->y);
}
void player_idle(Player *player) {
  
}
