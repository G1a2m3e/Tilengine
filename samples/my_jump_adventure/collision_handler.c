#include "collision_handler.h"
#include "../../src/LoadFile.h"
#include "../../src/LoadTMX.h"
#include "../../src/simplexml.h"
#include "Tilengine.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

bool swept_aabb(HitBox src, HitBox obstacle, float relative_dx,
                float relative_dy, float *hit_t) {
  // 初始已经重叠
  if (src.x < obstacle.x + obstacle.w && src.x + src.w > obstacle.x &&
      src.y < obstacle.y + obstacle.h && src.y + src.h > obstacle.y) {
    *hit_t = 0.0f;
    return true;
  }

  // Minkowski 扩张：将移动矩形转换为左上角的点
  float left = obstacle.x - src.w;
  float right = obstacle.x + obstacle.w;
  float top = obstacle.y - src.h;
  float bottom = obstacle.y + obstacle.h;

  float tx_entry, tx_exit;
  float ty_entry, ty_exit;

  // X轴进入和离开时间
  if (relative_dx == 0.0f) {
    if (src.x <= left || src.x >= right)
      return false;

    tx_entry = -INFINITY;
    tx_exit = INFINITY;
  } else {
    float t1 = (left - src.x) / relative_dx;
    float t2 = (right - src.x) / relative_dx;

    tx_entry = fminf(t1, t2);
    tx_exit = fmaxf(t1, t2);
  }

  // Y轴进入和离开时间
  if (relative_dy == 0.0f) {
    if (src.y <= top || src.y >= bottom)
      return false;

    ty_entry = -INFINITY;
    ty_exit = INFINITY;
  } else {
    float t1 = (top - src.y) / relative_dy;
    float t2 = (bottom - src.y) / relative_dy;

    ty_entry = fminf(t1, t2);
    ty_exit = fmaxf(t1, t2);
  }

  /**
    两个轴的时间区间求交集
    对于碰撞进入,两个轴都必须进入才算碰撞,所以取最大值
    对于碰撞离开,两个轴只要有一个离开就算离开,所以取最小值
  */
  float entry = fmaxf(tx_entry, ty_entry);
  float exit = fminf(tx_exit, ty_exit);

  if (entry > exit || exit < 0.0f || entry < 0.0f || entry > 1.0f) {
    return false;
  }

  *hit_t = entry;
  return true;
}

bool resolve_displacement(HitBox src, HitBox obs, float *src_dx, float *src_dy,
                          float *obs_dx, float *obs_dy, float hit_t) {
  // 1. 相对位移
  float rdx = *src_dx - *obs_dx;
  float rdy = *src_dy - *obs_dy;

  // 2. 计算各轴的碰撞进入时间
  float tx = -INFINITY;
  float ty = -INFINITY;

  /**
    src向右靠近obs，碰撞发生在src的右边界与obs的左边界接触时
    src.x + src.w + rdx * t = obs.x
    t = (obs.x - src.x - src.w) / rdx
    所以t的含义是一帧进行到多少比例时会发生碰撞，0表示碰撞发生在一帧的开始，1表示碰撞发生在一帧的结束
  */
  if (rdx > 0.0f)
    tx = (obs.x - src.x - src.w) / rdx;
  /**
    src向左靠近obs， 碰撞发生在src的左边界与obs的右边界接触时
    src.x + rdx * t = obs.x + obs.w
    t = (obs.x + obs.w - src.x) / rdx
  */
  else if (rdx < 0.0f)
    tx = (obs.x + obs.w - src.x) / rdx;

  /**
    y的计算同理
  */
  if (rdy > 0.0f)
    ty = (obs.y - src.y - src.h) / rdy;
  else if (rdy < 0.0f)
    ty = (obs.y + obs.h - src.y) / rdy;

  /**
    3. 确定碰撞轴和运动方向
    但是，仅仅在X方向接触，并不代表两个二维矩形已经发生碰撞。
    因为两个矩形要真正碰撞，必须同时满足X方向的投影发生重叠和Y方向的投影发生重叠
    所以首次碰撞时间是max(tx, ty)，也就是两个轴中进入时间较晚的那个时间点
    如果X轴进入时间更晚，说明最后造成两个矩形接触的是X方向，碰撞法线沿X轴也就是x_axis
    = true
  */
  bool x_axis = (tx >= ty);

  /**
    如果属于X方向碰撞，后续修正双方的dx
    如果属于Y方向碰撞，后续修正双方的dy
    例如角色斜向右下运动，撞到墙壁的左侧，那么只需要修正dx，dy可以保留，角色就能沿墙壁向下滑动
  */
  float *a = x_axis ? src_dx : src_dy;
  float *b = x_axis ? obs_dx : obs_dy;

  float relative = x_axis ? rdx : rdy;

  if (relative == 0.0f)
    return false;

  /**
    碰撞有六种情况,这里以水平方向举例:
    1. src从后面追上obs
      a. src和obs都向右 src_dx > 0, obs_dx > 0, rdx > 0 ----> va > 0, vb > 0
      b. src和obs都向左 src_dx < 0, obs_dx < 0, rdx < 0 ----> va > 0, vb > 0
    2. obs从后面追上src
      a. src和obs都向右 src_dx > 0, obs_dx > 0, rdx < 0 ----> va < 0, vb < 0
      b. src和obs都向左 src_dx < 0, obs_dx < 0, rdx > 0 ----> va < 0, vb < 0
    3. src在左往右,obs在右往左: src_dx > 0, obs_dx < 0, rdx > 0 ----> va > 0, vb
    < 0
    4. src在右往左,obs在左往右: src_dx < 0, obs_dx > 0, rdx < 0 ----> va > 0, vb
    < 0 下面va,vb的判断就是根据上面的规律得到,而始终va > vb
  */

  /**　
    direction = +1：src 相对于 obs 沿坐标轴正方向运动（X 轴向右，Y 轴向下）
    direction = -1：src 相对于 obs 沿坐标轴负方向运动（X 轴向左，Y 轴向上）
  */
  float direction = relative > 0.0f ? 1.0f : -1.0f;

  float va = *a * direction;
  float vb = *b * direction;

  if (va <= vb)
    return false;

  // 4. 获取推动权重
  float wa = src.push_weight;
  float wb = obs.push_weight;

  // 权重必须大于 0，可以使用 INFINITY
  if (!(wa > 0.0f) || !(wb > 0.0f))
    return false;

  if (isinf(wa) && isinf(wb))
    return false;

  // 5. 根据权重计算接触后的共同运动速度
  float v;

  if (isinf(wa)) {
    v = va;
  } else if (isinf(wb)) {
    v = vb;
  } else if (va >= 0.0f && vb >= 0.0f) {
    // src 从后面追上 obs
    float k = fminf(1.0f, wa / wb);
    v = vb + (va - vb) * k;
  } else if (va <= 0.0f && vb <= 0.0f) {
    // obs 从后面追上 src
    float k = fminf(1.0f, wb / wa);
    v = va + (vb - va) * k;
  } else {
    // 两个物体相向运动
    float balance = wa * va + wb * vb;

    if (balance >= 0.0f)
      v = fminf(va, balance / wb);
    else
      v = fmaxf(vb, balance / wa);
  }

  // 6. 仅修正碰撞之后的剩余位移
  float remaining = 1.0f - hit_t;

  *a += (v - va) * remaining * direction;
  *b += (v - vb) * remaining * direction;

  return true;
}

static void parse_xml(const char *filename, SimpleXmlTagHandler handler) {
  ssize_t size;
  char *xml = LoadFile(filename, &size);
  if (!xml)
    return;
  SimpleXmlParser parser = simpleXmlCreateParser(xml, size);
  if (parser) {
    simpleXmlParse(parser, handler);
    simpleXmlDestroyParser(parser);
  }
  free(xml);
}

static struct {
  CollisionOffset box;
  const char *object_name;
  bool name_matches, object_found;
  uint32_t gid, current_gid;
  int target_tile, current_tile;
  bool in_group, found;
} HitBox_loader;

static void *read_object_gid(SimpleXmlParser parser, SimpleXmlEvent event,
                             const char *tag, const char *attribute,
                             const char *value) {
  (void)parser;
  if (strcmp(tag, "object") != 0)
    return read_object_gid;
  if (event == ADD_SUBTAG) {
    HitBox_loader.name_matches = false;
    HitBox_loader.current_gid = 0;
  } else if (event == ADD_ATTRIBUTE) {
    if (strcmp(attribute, "name") == 0)
      HitBox_loader.name_matches =
          strcmp(value, HitBox_loader.object_name) == 0;
    else if (strcmp(attribute, "gid") == 0)
      HitBox_loader.current_gid = (uint32_t)strtoul(value, NULL, 10);
  } else if (event == FINISH_ATTRIBUTES && HitBox_loader.name_matches &&
             !HitBox_loader.object_found) {
    HitBox_loader.gid = HitBox_loader.current_gid & 0x0FFFFFFFu;
    HitBox_loader.object_found = true;
  }
  return read_object_gid;
}

/* Read only the first collision object belonging to the referenced tile. */
static void *read_HitBox(SimpleXmlParser parser, SimpleXmlEvent event,
                         const char *tag, const char *attribute,
                         const char *value) {
  (void)parser;
  if (event == ADD_SUBTAG && strcmp(tag, "objectgroup") == 0)
    HitBox_loader.in_group =
        HitBox_loader.current_tile == HitBox_loader.target_tile;
  else if (event == ADD_ATTRIBUTE) {
    if (strcmp(tag, "tile") == 0 && strcmp(attribute, "id") == 0)
      HitBox_loader.current_tile = atoi(value);
    else if (strcmp(tag, "object") == 0 && HitBox_loader.in_group &&
             !HitBox_loader.found) {
      float number = strtof(value, NULL);
      if (strcmp(attribute, "x") == 0)
        HitBox_loader.box.x_offset = number;
      else if (strcmp(attribute, "y") == 0)
        HitBox_loader.box.y_offset = number;
      else if (strcmp(attribute, "width") == 0)
        HitBox_loader.box.w = number;
      else if (strcmp(attribute, "height") == 0)
        HitBox_loader.box.h = number;
    }
  } else if (event == FINISH_ATTRIBUTES && strcmp(tag, "object") == 0 &&
             HitBox_loader.in_group)
    HitBox_loader.found = true;
  else if (event == FINISH_TAG) {
    if (strcmp(tag, "objectgroup") == 0)
      HitBox_loader.in_group = false;
    else if (strcmp(tag, "tile") == 0)
      HitBox_loader.current_tile = -1;
  }
  return read_HitBox;
}

CollisionOffset load_object_hitbox(const char *tmx_file,
                                   const char *object_name) {
  memset(&HitBox_loader, 0, sizeof(HitBox_loader));
  if (!tmx_file || !tmx_file[0] || !object_name || !object_name[0])
    return HitBox_loader.box;
  HitBox_loader.object_name = object_name;
  HitBox_loader.current_tile = -1;

  TMXInfo map = {0};
  if (!TMXLoad(tmx_file, &map))
    return HitBox_loader.box;
  parse_xml(tmx_file, read_object_gid);
  if (!HitBox_loader.gid)
    return HitBox_loader.box;

  /* The largest firstgid <= GID identifies its tileset. */
  TMXTileset *tileset = NULL;
  for (int i = 0; i < map.num_tilesets; ++i)
    if (HitBox_loader.gid >= (uint32_t)map.tilesets[i].firstgid &&
        (!tileset || map.tilesets[i].firstgid > tileset->firstgid))
      tileset = &map.tilesets[i];
  if (!tileset)
    return HitBox_loader.box;

  HitBox_loader.target_tile = HitBox_loader.gid - tileset->firstgid;
  FileInfo path;
  char filename[300];
  SplitFilename(tmx_file, &path);
  BuildFilePath(filename, sizeof(filename), path.path, tileset->source, NULL);
  parse_xml(filename, read_HitBox);
  return HitBox_loader.box;
}

// 某些图块允许玩家可以从底部穿过
static bool allow_pass_through(HitBox src, HitBox obstacle, float dy,
                               Tile tile) {
  // Terrain's Tiled IDs are zero-based; index is one-based, value includes
  // flags.
  int id = (int)tile.index - 1;
  bool one_way = tile.tileset == 0 &&
                 (id == 17 || id == 18 || id == 19 || id == 39 || id == 40 ||
                  id == 41 || id == 61 || id == 62 || id == 63);

  if (!one_way)
    return false;

  // 只有向下移动、且起始脚底位于平台顶面或上方时，才参与碰撞。
  return dy <= 0.0f || src.y + src.h > obstacle.y;
}

TLN_Tilemap get_collision_tilemap(void) {
  if (!TLN_GetContext())
    return NULL;

  for (int layer = 0; layer < TLN_GetNumLayers(); ++layer) {
    if (TLN_GetLayerType(layer) != LAYER_TILE)
      continue;
    TLN_Tilemap map = TLN_GetLayerTilemap(layer);
    if (map && map->num_tilesets > 0 && map->tilesets[0] && map->cols > 0 &&
        map->rows > 0 && map->tilewidth > 0 &&
        map->tileheight > 0)
      return map;
  }
  return NULL;
}

/* 允许沿墙滑动,所以需要x,y轴单独计算分量, x轴碰撞不影响y轴运动 */
HitBox handle_maps_collision(HitBox src, float dx, float dy) {
  if (dx == 0 && dy == 0)
    return src;

  TLN_Tilemap tilemap = get_collision_tilemap();
  if (!tilemap) {
    src.x += dx;
    src.y += dy;
    return src;
  }
  int tile_width = tilemap->tilewidth;
  int tile_height = tilemap->tileheight;

  /* Swept bounds, including tiles touched exactly at the rectangle edges. */
  int first_col = (int)ceilf(fminf(src.x, src.x + dx) / tile_width) - 1;
  int last_col = (int)floorf((fmaxf(src.x, src.x + dx) + src.w) / tile_width);
  int first_row = (int)ceilf(fminf(src.y, src.y + dy) / tile_height) - 1;
  int last_row = (int)floorf((fmaxf(src.y, src.y + dy) + src.h) / tile_height);
  if (first_col < 0)
    first_col = 0;
  if (last_col >= tilemap->cols)
    last_col = tilemap->cols - 1;
  if (first_row < 0)
    first_row = 0;
  if (last_row >= tilemap->rows)
    last_row = tilemap->rows - 1;

  for (int axis = 0; axis < 2; ++axis) {
    float vx = axis == 0 ? dx : 0;
    float vy = axis == 1 ? dy : 0;
    if (vx == 0 && vy == 0)
      continue;

    float nearest = 1.0f;
    float target = axis == 0 ? src.x + dx : src.y + dy;

    for (int row = first_row; row <= last_row; ++row) {
      for (int col = first_col; col <= last_col; ++col) {
        Tile cell = tilemap->tiles[row * tilemap->cols + col];
        if (cell.index == 0)
          continue;
        HitBox tile = {.x = col * tile_width,
                       .y = row * tile_height,
                       .w = tile_width,
                       .h = tile_height};
        if (allow_pass_through(src, tile, vy, cell))
          continue;
        float hit_t;
        if (swept_aabb(src, tile, vx, vy, &hit_t) && hit_t <= nearest) {
          nearest = hit_t;
          if (hit_t == 0)
            target = axis == 0 ? src.x : src.y;
          else if (axis == 0)
            target = dx > 0 ? tile.x - src.w : tile.x + tile.w;
          else
            target = dy > 0 ? tile.y - src.h : tile.y + tile.h;
        }
      }
    }
    if (axis == 0)
      src.x = target;
    else
      src.y = target;
  }
  return src;
}

bool is_colliding(HitBox box1, HitBox box2) {
  return (box1.x < box2.x + box2.w && box1.x + box1.w > box2.x &&
          box1.y < box2.y + box2.h && box1.y + box1.h > box2.y);
}

static float clampf(float v, float min, float max) {
  if (v < min)
    return min;
  if (v > max)
    return max;
  return v;
}

bool rect_circle_collision(HitBox rect, Circle circle) {
  // 矩形上距离圆心最近的点
  float nearest_x = clampf(circle.x, rect.x, rect.x + rect.w);

  float nearest_y = clampf(circle.y, rect.y, rect.y + rect.h);

  float dx = circle.x - nearest_x;
  float dy = circle.y - nearest_y;

  return dx * dx + dy * dy <= circle.radius * circle.radius;
}

static void update_hit(double t, double *best) {
  // 接受端点附近的舍入误差，输出时间仍限制在 [0, 1]。
  const double epsilon = 1e-9;
  if (t >= -epsilon && t <= 1.0 + epsilon) {
    t = fmax(0.0, fmin(t, 1.0));
    if (t < *best)
      *best = t;
  }
}

bool swept_aabb_rect_circle(HitBox src, Circle obstacle, float relative_dx,
                            float relative_dy, float *hit_t) {
  // 使用矩形局部坐标，减少较大世界坐标参与计算时的误差。
  double xmin = 0, xmax = src.w;
  double ymin = 0, ymax = src.h;
  double cx = (double)obstacle.x - src.x;
  double cy = (double)obstacle.y - src.y;
  double r = obstacle.radius;

  // 相对运动：固定矩形，反向移动圆心
  double vx = -(double)relative_dx;
  double vy = -(double)relative_dy;

  // 1. 初始重叠或相切都算碰到。
  double nx = fmax(xmin, fmin(cx, xmax));
  double ny = fmax(ymin, fmin(cy, ymax));

  double ox = cx - nx;
  double oy = cy - ny;

  double distance_sq = ox * ox + oy * oy;
  double contact_error = 64 * DBL_EPSILON * fmax(1.0, fmax(distance_sq, r * r));
  if (distance_sq <= r * r + contact_error) {
    *hit_t = 0.0f;
    return true;
  }

  double a = vx * vx + vy * vy;
  if (a == 0.0f)
    return false;

  double best = DBL_MAX;
  const double edge_epsilon = 1e-7;

  // 2. 检测左右两条扩张边
  if (vx > 0.0f) {
    double t = (xmin - r - cx) / vx;
    double y = cy + vy * t;

    if (y >= ymin - edge_epsilon && y <= ymax + edge_epsilon)
      update_hit(t, &best);
  } else if (vx < 0.0f) {
    double t = (xmax + r - cx) / vx;
    double y = cy + vy * t;

    if (y >= ymin - edge_epsilon && y <= ymax + edge_epsilon)
      update_hit(t, &best);
  }

  // 3. 检测上下两条扩张边
  if (vy > 0.0f) {
    double t = (ymin - r - cy) / vy;
    double x = cx + vx * t;

    if (x >= xmin - edge_epsilon && x <= xmax + edge_epsilon)
      update_hit(t, &best);
  } else if (vy < 0.0f) {
    double t = (ymax + r - cy) / vy;
    double x = cx + vx * t;

    if (x >= xmin - edge_epsilon && x <= xmax + edge_epsilon)
      update_hit(t, &best);
  }

  // 4. 检测四个圆角
  double corners[4][2] = {
      {xmin, ymin}, {xmax, ymin}, {xmin, ymax}, {xmax, ymax}};

  for (int i = 0; i < 4; ++i) {
    double px = cx - corners[i][0];
    double py = cy - corners[i][1];

    // |P + V*t|^2 = r^2
    // a*t^2 + 2*b*t + c = 0
    double b = px * vx + py * vy;
    double c = px * px + py * py - r * r;

    double discriminant = fma(-a, c, b * b);
    double error = 64 * DBL_EPSILON * fmax(1.0, b * b + fabs(a * c));
    if (discriminant < -error)
      continue;
    discriminant = fmax(0.0, discriminant);

    // 稳定求根，避免两个接近的数相减丢失精度。
    double q = -b - copysign(sqrt(discriminant), b);
    double t = q == 0 ? -b / a : fmin(q / a, c / q);
    update_hit(t, &best);
  }

  if (best == DBL_MAX)
    return false;

  *hit_t = (float)best;
  return true;
}