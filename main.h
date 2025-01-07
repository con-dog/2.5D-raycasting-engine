#ifndef MAIN_H
#define MAIN_H

// Player

// Input helpers
#define KEY_UP (1 << 0)
#define KEY_DOWN (1 << 1)
#define KEY_LEFT (1 << 2)
#define KEY_RIGHT (1 << 3)

// #define TEXTURE_PIXEL_W 64
// #define TEXTURE_PIXEL_H 64

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_rect.h>

#include "./assets/textures/constants.h"
#include "./config/constants.h"
#include "./data/grid/constants.h"
#include "./data/grid/types.h"
#include "./data/wall/types.h"
#include "./objects/types.h"
#include "./objects/player/constants.h"
#include "./types/algebraic-types.h"
#include "./utils/math-utils.h"

#endif
