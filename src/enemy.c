#include "enemy.h"
#include "player.h"
#include "raywhen.h"

// External enemy array
Enemy enemies[MAX_ENEMIES];
int numEnemies = 0;

void resetEnemies(void) {
    numEnemies = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].alive = 0;
    }
}

void addEnemy(double x, double y) {
    if (numEnemies < MAX_ENEMIES) {
        enemies[numEnemies].x = x;
        enemies[numEnemies].y = y;
        enemies[numEnemies].radius = 0.3;
        enemies[numEnemies].health = 1;
        enemies[numEnemies].alive = 1;
        numEnemies++;
    }
}

// Simple hitscan at crosshair against all enemies
void shootAtCrosshair(void) {
    int centerX = SCREEN_WIDTH / 2;
    double wallDist = depthBuffer ? depthBuffer[centerX] : 1e9;
    
    for (int i = 0; i < numEnemies; i++) {
        if (!enemies[i].alive) continue;
        
        // Compute relative angle and distance
        double dx = enemies[i].x - playerX;
        double dy = enemies[i].y - playerY;
        double dist = sqrt(dx*dx + dy*dy);
        if (dist < 0.0001) continue;
        
        double angleToEnemy = atan2(dy, dx);
        double rel = angleToEnemy - playerAngle;
        while (rel < -M_PI) rel += 2*M_PI;
        while (rel >  M_PI) rel -= 2*M_PI;

        // Projected half-width in radians roughly equals enemy.radius / distance
        double halfWidth = atan2(enemies[i].radius, dist);
        // Allow small aim assist margin
        double aimTolerance = halfWidth * 1.6;

        // Check if within FOV and aiming tolerance
        if (fabs(rel) <= aimTolerance && dist < wallDist) {
            enemies[i].health -= 1;
            if (enemies[i].health <= 0) enemies[i].alive = 0;
            break; // Only hit one enemy per shot
        }
    }
}

void renderEnemies(void) {
    // Render all enemies as billboard sprites, depth-tested against walls
    for (int i = 0; i < numEnemies; i++) {
        if (!enemies[i].alive) continue;
        
        double dx = enemies[i].x - playerX;
        double dy = enemies[i].y - playerY;
        double dist = sqrt(dx*dx + dy*dy);
        if (dist > 0.001) {
            double angleToEnemy = atan2(dy, dx);
            double rel = angleToEnemy - playerAngle;
            // Normalize to [-pi,pi]
            while (rel < -M_PI) rel += 2*M_PI;
            while (rel >  M_PI) rel -= 2*M_PI;
            // Only render if within FOV
            if (fabs(rel) < (FOV * 0.6)) {
                int spriteScreenX = (int)((rel + FOV/2) / FOV * SCREEN_WIDTH);
                // Projected size (simple) and vertical placement centered around horizon
                int spriteH = (int)(SCREEN_HEIGHT / dist);
                int spriteW = spriteH; // square billboard
                int horizon = SCREEN_HEIGHT / 2 + (int)pitchOffset;
                int top = horizon - spriteH/2;
                int left = spriteScreenX - spriteW/2;
                // Simple color and shading by distance
                uint32_t base = colorref_to_bgra(RGB(220, 40, 40));
                // Draw with depth test
                for (int sx = 0; sx < spriteW; ++sx) {
                    int xOnScreen = left + sx;
                    if (xOnScreen < 0 || xOnScreen >= SCREEN_WIDTH) continue;
                    // Occlusion: only draw if enemy in front of wall at this column
                    if (depthBuffer && dist >= depthBuffer[xOnScreen]) continue;
                    for (int sy = 0; sy < spriteH; ++sy) {
                        int yOnScreen = top + sy;
                        if (yOnScreen < 0 || yOnScreen >= SCREEN_HEIGHT) continue;
                        // Simple circular mask inside the rectangle to look less boxy
                        double nx = (sx - spriteW/2) / (double)(spriteW/2);
                        double ny = (sy - spriteH/2) / (double)(spriteH/2);
                        if (nx*nx + ny*ny > 1.0) continue;
                        backPixels[yOnScreen * SCREEN_WIDTH + xOnScreen] = base;
                    }
                }
            }
        }
    }
}
