#include "player.h"
#include "map.h"

// Player state
double playerX = 8.5, playerY = 8.5;
double playerAngle = 0.0;
double playerSpeed = MOVE_SPEED;
double rotationSpeed = ROT_SPEED;
double pitchOffset = 0.0; // vertical look in pixels (positive moves horizon down)
int mouseLookEnabled = 0;
double velX = 0.0, velY = 0.0; // velocity for sliding

// Precomputed trigonometric values for performance
static double cosAngle = 1.0, sinAngle = 0.0;
static double cosAngle90 = 0.0, sinAngle90 = 1.0;
static double cosAngle270 = 0.0, sinAngle270 = -1.0;

void setPlayerPosition(double x, double y) {
    playerX = x;
    playerY = y;
}

// Update precomputed trigonometric values for performance
static void updateTrigValues(void) {
    cosAngle = cos(playerAngle);
    sinAngle = sin(playerAngle);
    cosAngle90 = cos(playerAngle - M_PI/2);
    sinAngle90 = sin(playerAngle - M_PI/2);
    cosAngle270 = cos(playerAngle + M_PI/2);
    sinAngle270 = sin(playerAngle + M_PI/2);
}

void updatePlayerMovement(int keys[256]) {
    // Handle continuous input with improved responsiveness
    if (keys[VK_LEFT]) {
        playerAngle -= rotationSpeed;
        updateTrigValues(); // Update trig values when angle changes
    }
    if (keys[VK_RIGHT]) {
        playerAngle += rotationSpeed;
        updateTrigValues(); // Update trig values when angle changes
    }
    
    // Acceleration-based movement (supports sliding) - using precomputed values
    double accel = ACCEL * (keys[VK_SHIFT] ? RUN_MULTIPLIER : 1.0);
    if (keys[VK_UP] || keys['W']) {
        velX += cosAngle * accel;
        velY += sinAngle * accel;
    }
    if (keys[VK_DOWN] || keys['S']) {
        velX -= cosAngle * accel;
        velY -= sinAngle * accel;
    }
    if (keys['A']) {
        velX += cosAngle90 * accel;
        velY += sinAngle90 * accel;
    }
    if (keys['D']) {
        velX += cosAngle270 * accel;
        velY += sinAngle270 * accel;
    }

    // Clamp speed
    double speed = sqrt(velX*velX + velY*velY);
    double maxSpd = MAX_SPEED * (keys[VK_SHIFT] ? RUN_MULTIPLIER : 1.0);
    if (speed > maxSpd) {
        velX = velX * (maxSpd / speed);
        velY = velY * (maxSpd / speed);
    }

    // Apply friction (lighter when input is held for slide feel)
    double fr = (keys[VK_UP]||keys['W']||keys[VK_DOWN]||keys['S']||keys['A']||keys['D']) ? SLIDE_FRICTION : FRICTION;
    velX *= fr;
    velY *= fr;

    // Move with collision and wall sliding
    double newX = playerX + velX;
    double newY = playerY + velY;
    if (canMoveTo(newX, playerY)) playerX = newX; else velX = 0;
    if (canMoveTo(playerX, newY)) playerY = newY; else velY = 0;
    
    // Speed handled via accel/max speed; maintain legacy variable for any other uses
    playerSpeed = MOVE_SPEED * (keys[VK_SHIFT] ? 2.0 : 1.0);
}

void handleMouseLook(HWND hwnd, int dx, int dy) {
    if (mouseLookEnabled) {
        playerAngle += dx * MOUSE_SENS;
        updateTrigValues(); // Update trig values when angle changes
        pitchOffset += -(double)dy * PITCH_SENS;
        // clamp pitch offset to avoid excessive tilt
        double maxPitch = SCREEN_HEIGHT * 0.45;
        if (pitchOffset < -maxPitch) pitchOffset = -maxPitch;
        if (pitchOffset >  maxPitch) pitchOffset =  maxPitch;
    }
}
