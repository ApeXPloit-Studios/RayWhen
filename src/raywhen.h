#ifndef RAYWHEN_H
#define RAYWHEN_H

#include <windows.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Configuration constants
#define MIN_SCREEN_WIDTH 640
#define MIN_SCREEN_HEIGHT 480
#define DEFAULT_SCREEN_WIDTH 1024
#define DEFAULT_SCREEN_HEIGHT 768
#define MAP_WIDTH 16
#define MAP_HEIGHT 16
#define MAX_TEXTURES 16
#define TEX_WIDTH 64
#define TEX_HEIGHT 64
#define FOV (M_PI / 3.0)  // 60 degrees
#define MOVE_SPEED 0.08
#define ROT_SPEED 0.05
#define MAX_DISTANCE 20.0
#define RAY_STEP 0.01
#define TARGET_FPS 30
#define FRAME_TIME (1000 / TARGET_FPS)  // 33ms for 30 FPS
#define MOUSE_SENS 0.01
#define PITCH_SENS 0.4   // pixels per mouse unit (scaled later)

// Movement physics
#define ACCEL 0.06
#define FRICTION 0.92
#define SLIDE_FRICTION 0.98
#define MAX_SPEED 0.22
#define RUN_MULTIPLIER 1.6

// Global screen dimensions (will be updated on resize)
extern int SCREEN_WIDTH;
extern int SCREEN_HEIGHT;

// Back buffer for double buffering
extern HDC backDC;
extern HBITMAP backBMP;
extern HBITMAP backOldBMP;
extern int backW;
extern int backH;
extern uint32_t *backPixels; // BGRA top-down

// Depth buffer for sprite occlusion
extern double *depthBuffer;
extern int depthW;

// Utility functions
static inline uint32_t colorref_to_bgra(COLORREF c) {
    return ((uint32_t)GetBValue(c)) | (((uint32_t)GetGValue(c)) << 8) | (((uint32_t)GetRValue(c)) << 16) | 0xFF000000u;
}

// Function declarations
void ensureBackBuffer(HWND hwnd);
void parseLaunchArgs(void);
void drawDebugInfo(HDC hdc);

#endif // RAYWHEN_H
