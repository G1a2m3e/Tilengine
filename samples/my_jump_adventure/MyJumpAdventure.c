#include "../../src/LoadFile.h"
#include "Tilengine.h"
#include "game.h"
#include "env_interaction.h"
#include "traps.h"
#include "utils.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
  关卡设计要求:
  1. 玩家叫virtual_guy
  2. 水果单独放在对象层fruits中
*/
#define BUTTON_SPRITE 128
#define BUTTON_COUNT 3
#define TRANSITION_SPRITE (BUTTON_SPRITE + BUTTON_COUNT)
#define BUTTON_WIDTH 20
#define BUTTON_HEIGHT 20
#define BUTTON_MARGIN 3
#define TRANSITION_MS 350

static int requested_level;
static struct {
  TLN_Spriteset set;
  int x;
  bool enabled;
} level_buttons[BUTTON_COUNT];
static const int level_offsets[BUTTON_COUNT] = {-1, 1, 0};
static TLN_Spriteset transition_set;
static uint32_t transition_started;
static enum { PLAYING, CLOSING, SWITCHING, OPENING } transition_state;
static void main_loop(uint32_t frame);
static int world_width;
static int world_height;

void clear_all_sprites(void) {
  int count = TLN_GetNumSprites();

  for (int i = 0; i < count; ++i) {
    if (i >= BUTTON_SPRITE)
      continue;
    TLN_DisableSpriteAnimation(i);
    TLN_DisableSprite(i);
  }
}

/* Transition.png is a 44x44 diamond; the largest size covers every corner. */
static void show_transition(float progress) {
  if (progress <= 0) {
    TLN_ResetSpriteScaling(TRANSITION_SPRITE);
    TLN_SetSpritePosition(TRANSITION_SPRITE, -44, -44);
    return;
  }
  float size = (TLN_GetWidth() + TLN_GetHeight()) * 1.1f * progress;
  TLN_SetSpriteScaling(TRANSITION_SPRITE, size / 44.0f, size / 44.0f);
  TLN_SetSpritePosition(TRANSITION_SPRITE,
                        (int)((TLN_GetWidth() - size) * 0.5f),
                        (int)((TLN_GetHeight() - size) * 0.5f));
}

static bool level_exists(int level) {
  return level > 0 && CheckFile(get_level_file_name(level));
}

static void update_level_buttons(void) {
  for (int i = 0; i < BUTTON_COUNT; ++i) {
    level_buttons[i].enabled =
        (i == 2 || level_exists(get_current_level() + level_offsets[i]));
    TLN_SetSpriteBlendMode(BUTTON_SPRITE + i,
                           level_buttons[i].enabled ? BLEND_NONE : BLEND_MIX50,
                           0);
  }
}

static void change_level(int level) {
  if (transition_state != PLAYING || !level_exists(level))
    return;
  requested_level = level;
  transition_state = CLOSING;
  transition_started = TLN_GetTicks();
  show_transition(0);
}

static void on_mouse_event(SDL_Event *event) {
  if (event->type != SDL_MOUSEBUTTONDOWN ||
      event->button.button != SDL_BUTTON_LEFT || transition_state != PLAYING)
    return;

  int width = TLN_GetWindowWidth(), height = TLN_GetWindowHeight();
  int window_width = width, window_height = height;
  SDL_Window *window = SDL_GetWindowFromID(event->button.windowID);
  if (window)
    SDL_GetWindowSize(window, &window_width, &window_height);
  int x =
      (event->button.x - (window_width - width) / 2) * TLN_GetWidth() / width;
  int y = (event->button.y - (window_height - height) / 2) * TLN_GetHeight() /
          height;
  for (int i = 0; i < BUTTON_COUNT; ++i) {
    if (level_buttons[i].enabled && y >= BUTTON_MARGIN &&
        y < BUTTON_MARGIN + BUTTON_HEIGHT && x >= level_buttons[i].x &&
        x < level_buttons[i].x + BUTTON_WIDTH) {
      change_level(get_current_level() + level_offsets[i]);
      return;
    }
  }
}

static bool init_level_buttons(void) {
  const char *files[] = {"Menu/Buttons/Previous.png", "Menu/Buttons/Next.png",
                         "Menu/Buttons/Restart.png"};
  for (int i = 0; i < BUTTON_COUNT; ++i) {
    level_buttons[i].set = TLN_LoadSpriteset(files[i]);
    if (!level_buttons[i].set)
      return false;
    level_buttons[i].x = TLN_GetWidth() - BUTTON_MARGIN - BUTTON_WIDTH -
                         (BUTTON_COUNT - 1 - i) * (BUTTON_WIDTH + BUTTON_MARGIN);
    TLN_ConfigSprite(BUTTON_SPRITE + i, level_buttons[i].set, FLAG_PRIORITY);
    TLN_EnableSpriteCollision(BUTTON_SPRITE + i, false);
    // TLN_SetSpriteScaling(BUTTON_SPRITE + i, 1, 1);
    TLN_SetSpritePosition(BUTTON_SPRITE + i, level_buttons[i].x, BUTTON_MARGIN);
  }
  update_level_buttons();
  return true;
}

int main(void) {
  t_trap_factory_init();
  TLN_SetLoadPath("assets/my_jump_adventure");

  load_level(1);
  if (!init_level_buttons()) {
    fprintf(stderr, "Failed to load level buttons: %s\n",
            TLN_GetErrorString(TLN_GetLastError()));
    return 1;
  }
  transition_set = TLN_LoadSpriteset("Other/Transition.png");
  if (!transition_set) {
    fprintf(stderr, "Failed to load transition: %s\n",
            TLN_GetErrorString(TLN_GetLastError()));
    return 1;
  }
  TLN_ConfigSprite(TRANSITION_SPRITE, transition_set, FLAG_PRIORITY);
  TLN_EnableSpriteCollision(TRANSITION_SPRITE, false);
  show_transition(0);

  TLN_CreateWindow(NULL, 0);
  TLN_SetSDLCallback(on_mouse_event);
  reset_game_timer();
  TLN_SetMainTask(main_loop);
  TLN_SetSDLCallback(NULL);
  TLN_DisableSprite(TRANSITION_SPRITE);
  TLN_DeleteSpriteset(transition_set);
  for (int i = 0; i < BUTTON_COUNT; ++i) {
    TLN_DisableSprite(BUTTON_SPRITE + i);
    TLN_DeleteSpriteset(level_buttons[i].set);
  }
  return 0;
}

static void main_loop(uint32_t frame) {
  if (transition_state == SWITCHING) {
    clear_all_sprites();
    TLN_ReleaseWorld();
    load_level(requested_level);
    update_level_buttons();
    render(frame);
    transition_state = OPENING;
    transition_started = TLN_GetTicks();
    return;
  }
  if (transition_state != PLAYING) {
    float progress =
        (TLN_GetTicks() - transition_started) / (float)TRANSITION_MS;
    if (progress < 1.0f) {
      show_transition(transition_state == CLOSING ? progress : 1.0f - progress);
    } else if (transition_state == CLOSING) {
      show_transition(1);
      transition_state = SWITCHING; // Present the fully covered frame first.
    } else {
      show_transition(0);
      reset_game_timer();
      transition_state = PLAYING;
    }
    return;
  }

  bool switch_level = !update(frame);
  render(frame);
  if (switch_level)
    change_level(get_current_level() + (player_is_dead(get_player()) ? 0 : 1));
}
