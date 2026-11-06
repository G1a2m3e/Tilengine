#include "fruit.h"

static TLN_Spriteset collected_set;
static TLN_Sequence collected_seq;

void collect(Fruit *fruit) {
  if (!fruit || !fruit->object || fruit->collected)
    return;

  fruit->collected = true;
  fruit->object->visible = false;
  fruit->collect_sprite = -1;

  // All pickup effects share these resources; only sprite slots are temporary.
  if (!collected_set)
    collected_set = TLN_LoadSpriteset("Items/Fruits/Collected.png");
  if (!collected_set)
    return;
  if (!collected_seq)
    collected_seq =
        TLN_CreateSpriteSequence("collected", collected_set, "collected", 3);
  if (!collected_seq)
    return;

  int sprite = TLN_GetAvailableSprite();
  if (sprite < 0)
    return;

  if (!TLN_SetSpriteSet(sprite, collected_set))
    return;

  TLN_SetSpriteFlags(sprite, FLAG_NONE);
  TLN_EnableSpriteCollision(sprite, false);
  if (!TLN_SetSpriteWorldPosition(sprite, fruit->object->x, fruit->object->y) ||
      !TLN_SetSpriteAnimation(sprite, collected_seq, 1)) {
    TLN_DisableSpriteAnimation(sprite);
    TLN_DisableSprite(sprite);
    return;
  }
  fruit->collect_sprite = sprite;
}

void update_fruit(Fruit *fruit) {
  if (!fruit || !fruit->collected || fruit->collect_sprite < 0)
    return;

  if (!TLN_GetAnimationState(fruit->collect_sprite)) {
    TLN_DisableSpriteAnimation(fruit->collect_sprite);
    TLN_DisableSprite(fruit->collect_sprite);
    fruit->collect_sprite = -1;
  }
}
