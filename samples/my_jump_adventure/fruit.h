#ifndef FRUITS_H
#define FRUITS_H

#include "../../src/ObjectList.h"
#include "collision_handler.h"

typedef struct {
  HitBox hitbox;
  bool collected;
  TLN_Object *object; // 关联的对象指针
  int collect_sprite; // 拾取特效精灵槽位，-1 表示没有特效
} Fruit;

extern void collect(Fruit *fruit);
extern void update_fruit(Fruit *fruit);

#endif
