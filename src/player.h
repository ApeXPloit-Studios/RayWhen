#ifndef PLAYER_H
#define PLAYER_H

#include "raywhen.h"

// Player state
extern double playerX, playerY;
extern double playerAngle;
extern double playerSpeed;
extern double rotationSpeed;
extern double pitchOffset; // vertical look in pixels (positive moves horizon down)
extern int mouseLookEnabled;
extern double velX, velY; // velocity for sliding

// Function declarations
void setPlayerPosition(double x, double y);
void updatePlayerMovement(int keys[256]);
void handleMouseLook(HWND hwnd, int dx, int dy);

#endif // PLAYER_H
