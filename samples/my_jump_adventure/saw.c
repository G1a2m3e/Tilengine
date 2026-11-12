#include "saw.h"
#include "utils.h"
#include <math.h>

struct _TSaw {
  TTrap parent_instance;
  MotionPath path;
  int target;
  int direction;
  bool joining;
  PathPoint join_point;
};

G_DEFINE_TYPE(TSaw, t_saw, T_TYPE_TRAP)

static void saw_pre_update(Sprite *sprite, uint32_t elapsed) {
  TSaw *self = T_SAW(sprite);
  SpriteData *data = get_sprite_data(sprite);
  data->desired_dx = data->desired_dy = 0;
  if (self->path.count < 2)
    return;

  float x = data->rect.x + data->rect.w * 0.5f;
  float y = data->rect.y + data->rect.h * 0.5f;
  float start_x = x, start_y = y;
  float remaining = self->path.speed * elapsed / 1000.0f;
  while (remaining > 0) {
    PathPoint goal =
        self->joining ? self->join_point : self->path.points[self->target];
    float dx = goal.x - x, dy = goal.y - y;
    float distance = hypotf(dx, dy);
    if (distance > remaining) {
      x += dx / distance * remaining;
      y += dy / distance * remaining;
      break;
    }

    x = goal.x;
    y = goal.y;
    remaining -= distance;
    if (self->joining) {
      self->joining = false;
      continue;
    }
    if (self->path.mode == PATH_LOOP) {
      self->target = (self->target + 1) % self->path.count;
    } else {
      if (self->target == self->path.count - 1)
        self->direction = -1;
      else if (self->target == 0)
        self->direction = 1;
      self->target += self->direction;
    }
  }
  data->desired_dx = x - start_x;
  data->desired_dy = y - start_y;
}

static void saw_post_update(Sprite *sprite, uint32_t elapsed) {
  g_return_if_fail(T_IS_SAW(sprite));
  SpriteData *data = get_sprite_data(sprite);
  data->resolved_dx = data->desired_dx;
  data->resolved_dy = data->desired_dy;
  data->rect.x += data->resolved_dx;
  data->rect.y += data->resolved_dy;
}
static void render_saw(Sprite *sprite, uint32_t elapsed) {
  g_return_if_fail(T_IS_SAW(sprite));
  SpriteData *data = get_sprite_data(sprite);
  TLN_SetSpriteWorldPosition(data->sprite_index, data->rect.x, data->rect.y);
}
static void saw_ready(Sprite *sprite) {
  TSaw *self = T_SAW(sprite);
  SpriteData *data = get_sprite_data(sprite);
  free_object_path(&self->path);
  self->joining = false;
  if (!load_object_path(data->object_id, &self->path))
    return;

  // 从初始中心寻找最近的路径段，不修改出生位置。
  float x = data->rect.x + data->rect.w * 0.5f;
  float y = data->rect.y + data->rect.h * 0.5f;
  float nearest = INFINITY;
  int segments =
      self->path.mode == PATH_LOOP ? self->path.count : self->path.count - 1;
  self->direction = 1;
  for (int i = 0; i < segments; ++i) {
    int next = (i + 1) % self->path.count;
    PathPoint a = self->path.points[i], b = self->path.points[next];
    float dx = b.x - a.x, dy = b.y - a.y;
    float length_sq = dx * dx + dy * dy;
    if (length_sq == 0)
      continue;
    float t = fmaxf(0, fminf(1, ((x - a.x) * dx + (y - a.y) * dy) / length_sq));
    PathPoint point = {a.x + t * dx, a.y + t * dy};
    float distance = hypotf(point.x - x, point.y - y);
    if (distance < nearest) {
      nearest = distance;
      self->target = next;
      self->join_point = point;
    }
  }
  self->joining = nearest > 0;
}

static void t_saw_finalize(GObject *object) {
  free_object_path(&T_SAW(object)->path);
  G_OBJECT_CLASS(t_saw_parent_class)->finalize(object);
}

static void t_saw_class_init(TSawClass *klass) {
  G_OBJECT_CLASS(klass)->finalize = t_saw_finalize;
  SpriteClass *sprite_class = T_SPRITE_CLASS(klass);
  sprite_class->sprite_ready = saw_ready;
  sprite_class->sprite_pre_update = saw_pre_update;
  sprite_class->sprite_post_update = saw_post_update;
  sprite_class->sprite_render = render_saw;
}

static void t_saw_init(TSaw *self) { self->path = (MotionPath){0}; }
