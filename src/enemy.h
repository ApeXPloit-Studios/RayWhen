#ifndef ENEMY_H
#define ENEMY_H

#include "raywhen.h"

// Enemy structure
typedef struct {
    double x;
    double y;
    double radius;
    int health;
    int alive;
} Enemy;

#define MAX_ENEMIES 10

// External enemy array
extern Enemy enemies[MAX_ENEMIES];
extern int numEnemies;

// Function declarations
void resetEnemies(void);
void addEnemy(double x, double y);
void shootAtCrosshair(void);
void renderEnemies(void);

#endif // ENEMY_H
