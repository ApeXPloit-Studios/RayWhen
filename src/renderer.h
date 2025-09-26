#ifndef RENDERER_H
#define RENDERER_H

#include "raywhen.h"

// Raycasting result structure
typedef struct {
    double distance;
    int wallType;
    int side; // 0 for horizontal walls, 1 for vertical walls
    double wallX; // Where on the wall the ray hit (for texture mapping)
} RayResult;

// Performance mode: flat-shaded walls (no per-pixel texturing)
extern int simpleShadingMode;
extern int perfExplicitlySet; // set to 1 if -perf/--no-perf provided

// Function declarations
RayResult castRay(double angle);
void renderScene(HDC hdc);
void renderMinimap(HDC hdc);

#endif // RENDERER_H
