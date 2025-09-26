#include "renderer.h"
#include "texture.h"
#include "map.h"
#include "player.h"
#include "enemy.h"
#include "dx11_renderer.h"

// External wallColors from texture.c
extern COLORREF wallColors[];

// Performance mode: flat-shaded walls (no per-pixel texturing)
int simpleShadingMode = 0;
int perfExplicitlySet = 0; // set to 1 if -perf/--no-perf provided

// No adaptive quality - performance mode is only set via command line

// Improved DDA raycasting algorithm with optimized trigonometric calculations
RayResult castRay(double angle) {
    // Use fast approximation for sin/cos if angle is close to common values
    double sinA, cosA;
    
    // Check for common angles to avoid expensive trig calculations
    if (fabs(angle) < 0.001) {
        sinA = 0.0;
        cosA = 1.0;
    } else if (fabs(angle - M_PI/2) < 0.001) {
        sinA = 1.0;
        cosA = 0.0;
    } else if (fabs(angle - M_PI) < 0.001) {
        sinA = 0.0;
        cosA = -1.0;
    } else if (fabs(angle - 3*M_PI/2) < 0.001) {
        sinA = -1.0;
        cosA = 0.0;
    } else {
        sinA = sin(angle);
        cosA = cos(angle);
    }
    
    double rayPosX = playerX;
    double rayPosY = playerY;
    
    double deltaDistX = fabs(1.0 / cosA);
    double deltaDistY = fabs(1.0 / sinA);
    
    int mapX = (int)rayPosX;
    int mapY = (int)rayPosY;
    
    double sideDistX, sideDistY;
    
    int stepX, stepY;
    int side;
    
    if (cosA < 0) {
        stepX = -1;
        sideDistX = (rayPosX - mapX) * deltaDistX;
    } else {
        stepX = 1;
        sideDistX = (mapX + 1.0 - rayPosX) * deltaDistX;
    }
    
    if (sinA < 0) {
        stepY = -1;
        sideDistY = (rayPosY - mapY) * deltaDistY;
    } else {
        stepY = 1;
        sideDistY = (mapY + 1.0 - rayPosY) * deltaDistY;
    }
    
    // DDA algorithm
    int hit = 0;
    while (!hit) {
        if (sideDistX < sideDistY) {
            sideDistX += deltaDistX;
            mapX += stepX;
            side = 0;
        } else {
            sideDistY += deltaDistY;
            mapY += stepY;
            side = 1;
        }
        
        if (mapX < 0 || mapX >= MAP_WIDTH || mapY < 0 || mapY >= MAP_HEIGHT) {
            break;
        }
        
        if (map[mapY][mapX] > 0) {
            hit = 1;
        }
    }
    
    RayResult result;
    if (hit) {
        if (side == 0) {
            result.distance = (mapX - rayPosX + (1 - stepX) / 2) / cosA;
            result.wallX = rayPosY + result.distance * sinA;
        } else {
            result.distance = (mapY - rayPosY + (1 - stepY) / 2) / sinA;
            result.wallX = rayPosX + result.distance * cosA;
        }
        
        result.wallX -= floor(result.wallX); // Keep only fractional part
        
        result.wallType = map[mapY][mapX];
        result.side = side;
    } else {
        result.distance = MAX_DISTANCE;
        result.wallType = 1;
        result.side = 0;
        result.wallX = 0.0;
    }
    
    return result;
}

void renderScene(HDC hdc) {
    // Dispatch to appropriate renderer
    if (currentRenderer == RENDERER_DX11) {
        renderSceneDX11(GetParent(hdc));
        // If DirectX 11 renderer returns without rendering,
        // fall back to software renderer to avoid black screen
        // Continue with software renderer below
    }
    
    // Software renderer (existing code)
    // Fast clear sky/floor directly into backPixels
    if (!backPixels) return;
    const uint32_t sky = colorref_to_bgra(RGB(135, 206, 235));
    const uint32_t floorCol = colorref_to_bgra(RGB(60, 60, 60));
    int horizon = SCREEN_HEIGHT / 2 + (int)pitchOffset;
    if (horizon < 0) horizon = 0;
    if (horizon > SCREEN_HEIGHT) horizon = SCREEN_HEIGHT;
    for (int y = 0; y < horizon; ++y) {
        uint32_t *row = backPixels + y * SCREEN_WIDTH;
        for (int x = 0; x < SCREEN_WIDTH; ++x) row[x] = sky;
    }
    
    // Render textured floor
    for (int y = horizon; y < SCREEN_HEIGHT; ++y) {
        uint32_t *row = backPixels + y * SCREEN_WIDTH;
        
        // Calculate floor distance for this row
        double rowDistance = (SCREEN_HEIGHT / 2.0) / (y - SCREEN_HEIGHT / 2.0 - pitchOffset);
        
        for (int x = 0; x < SCREEN_WIDTH; ++x) {
            // Calculate floor intersection point
            double rayAngle = playerAngle - FOV/2 + FOV * x / SCREEN_WIDTH;
            double floorX = playerX + rowDistance * cos(rayAngle);
            double floorY = playerY + rowDistance * sin(rayAngle);
            
            // Get floor texture for this position
            int mapX = (int)floorX;
            int mapY = (int)floorY;
            
            if (mapX >= 0 && mapX < MAP_WIDTH && mapY >= 0 && mapY < MAP_HEIGHT) {
                int floorTextureId = mapFloorTextures[mapY][mapX];
                if (floorTextureId >= 0 && floorTextureId < MAX_TEXTURES && textures[floorTextureId].loaded) {
                    // Calculate texture coordinates
                    double texX = floorX - mapX;
                    double texY = floorY - mapY;
                    
                    int texXInt = (int)(texX * TEX_WIDTH) % TEX_WIDTH;
                    int texYInt = (int)(texY * TEX_HEIGHT) % TEX_HEIGHT;
                    if (texXInt < 0) texXInt += TEX_WIDTH;
                    if (texYInt < 0) texYInt += TEX_HEIGHT;
                    
                    COLORREF texColor = textures[floorTextureId].pixels[texYInt * TEX_WIDTH + texXInt];
                    
                    // Apply distance-based darkening
                    double darkenFactor = 1.0 / (1.0 + rowDistance * 0.1);
                    if (darkenFactor < 0.3) darkenFactor = 0.3;
                    
                    int r = (int)(GetRValue(texColor) * darkenFactor);
                    int g = (int)(GetGValue(texColor) * darkenFactor);
                    int b = (int)(GetBValue(texColor) * darkenFactor);
                    
                    row[x] = colorref_to_bgra(RGB(r, g, b));
                } else {
                    // Fallback to solid color
                    row[x] = floorCol;
                }
            } else {
                row[x] = floorCol;
            }
        }
    }

    // Walls (improved raycasting)
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        double rayAngle = playerAngle - FOV/2 + FOV * x / SCREEN_WIDTH;
        RayResult ray = castRay(rayAngle);
        
        double perpWallDist = ray.distance;  // This is already the perpendicular distance
        
        if (depthBuffer && x >= 0 && x < SCREEN_WIDTH) depthBuffer[x] = perpWallDist;

        int wallHeight = (int)(SCREEN_HEIGHT / perpWallDist);
        int start = horizon - wallHeight/2;
        int end   = start + wallHeight;

        // Clamp vertical bounds
        if (start < 0) start = 0;
        if (end > SCREEN_HEIGHT) end = SCREEN_HEIGHT;

        if (simpleShadingMode) {
            // Flat shading per column (compute once)
            COLORREF base = wallColors[ray.wallType];
            double shade = 1.0 - (perpWallDist / MAX_DISTANCE) * 0.7;
            if (ray.side == 1) shade *= 0.7;
            int r = (int)(GetRValue(base) * shade);
            int g = (int)(GetGValue(base) * shade);
            int b = (int)(GetBValue(base) * shade);
            uint32_t px = ((uint32_t)b) | (((uint32_t)g) << 8) | (((uint32_t)r) << 16) | 0xFF000000u;
            uint32_t *col = backPixels + start * SCREEN_WIDTH + x;
            for (int y = start; y < end; ++y) {
                *col = px;
                col += SCREEN_WIDTH;
            }
        } else {
            // Textured walls using asset textures with perspective-correct mapping
            double wallHeightFloat = (double)(end - start);
            
            // Get texture ID for this wall position
            int mapX = (int)(perpWallDist * cos(rayAngle) + playerX);
            int mapY = (int)(perpWallDist * sin(rayAngle) + playerY);
            if (mapX < 0) mapX = 0; if (mapX >= MAP_WIDTH) mapX = MAP_WIDTH - 1;
            if (mapY < 0) mapY = 0; if (mapY >= MAP_HEIGHT) mapY = MAP_HEIGHT - 1;
            int wallType = map[mapY][mapX];
            int textureId = mapTextures[mapY][mapX];
            if (textureId < 0 || textureId >= MAX_TEXTURES) textureId = 0;
            
            // Calculate wall intersection point properly
            double wallX;
            if (ray.side == 0) {
                // Hit vertical wall
                wallX = playerY + perpWallDist * sin(rayAngle);
            } else {
                // Hit horizontal wall
                wallX = playerX + perpWallDist * cos(rayAngle);
            }
            wallX -= floor(wallX);
            
            // Calculate texture X coordinate
            int texX = (int)(wallX * TEX_WIDTH);
            
            // Flip texture for proper orientation
            if ((ray.side == 0 && cos(rayAngle) > 0) || (ray.side == 1 && sin(rayAngle) < 0)) {
                texX = TEX_WIDTH - texX - 1;
            }
            
            // Clamp texture coordinates
            if (texX < 0) texX = 0; 
            if (texX >= TEX_WIDTH) texX = TEX_WIDTH - 1;
            
            // Use the same distance for texture mapping as wall height
            double perpWallDist = ray.distance;
            
            // Simple direct texture mapping
            for (int y = start; y < end; y++) {
                // Map screen Y to texture Y directly
                int texYInt = ((y - start) * TEX_HEIGHT) / (end - start);
                if (texYInt < 0) texYInt = 0; if (texYInt >= TEX_HEIGHT) texYInt = TEX_HEIGHT - 1;
                
                COLORREF texColor;
                if (textures[textureId].loaded) {
                    // Choose mipmap level based on perpendicular wall distance
                    if (perpWallDist > 6.0) {
                        // Use mipmap for distant walls
                        int mipTexX = texX / 2;
                        int mipTexY = texYInt / 2;
                        
                        // Clamp mipmap coordinates
                        if (mipTexX < 0) mipTexX = 0; if (mipTexX >= TEX_WIDTH/2) mipTexX = TEX_WIDTH/2 - 1;
                        if (mipTexY < 0) mipTexY = 0; if (mipTexY >= TEX_HEIGHT/2) mipTexY = TEX_HEIGHT/2 - 1;
                        
                        texColor = textures[textureId].pixels_mip[mipTexY * (TEX_WIDTH/2) + mipTexX];
                    } else {
                        // Use full resolution texture for close walls
                        texColor = textures[textureId].pixels[texYInt * TEX_WIDTH + texX];
                    }
                } else {
                    // Fallback to procedural color
                    texColor = getTextureColor(wallType, wallX, (double)texYInt / TEX_HEIGHT);
                }
                
                double shade = 1.0 - (perpWallDist / MAX_DISTANCE) * 0.7;
                if (ray.side == 1) shade *= 0.7;
                int r = (int)(GetRValue(texColor) * shade);
                int g = (int)(GetGValue(texColor) * shade);
                int b = (int)(GetBValue(texColor) * shade);
                backPixels[y * SCREEN_WIDTH + x] = ((uint32_t)b) | (((uint32_t)g) << 8) | (((uint32_t)r) << 16) | 0xFF000000u;
            }
        }
    }

    // Crosshair (simple lines at screen center)
    int cx = SCREEN_WIDTH / 2;
    int cy = SCREEN_HEIGHT / 2 + (int)pitchOffset;
    int chLen = 8;
    uint32_t chCol = colorref_to_bgra(RGB(255,255,255));
    for (int dx = -chLen; dx <= chLen; ++dx) {
        int xx = cx + dx; int yy = cy;
        if (xx >= 0 && xx < SCREEN_WIDTH && yy >= 0 && yy < SCREEN_HEIGHT) {
            backPixels[yy * SCREEN_WIDTH + xx] = chCol;
        }
    }
    for (int dy = -chLen; dy <= chLen; ++dy) {
        int xx = cx; int yy = cy + dy;
        if (xx >= 0 && xx < SCREEN_WIDTH && yy >= 0 && yy < SCREEN_HEIGHT) {
            backPixels[yy * SCREEN_WIDTH + xx] = chCol;
        }
    }

    // Render enemies
    renderEnemies();

    // HUD gun (simple rectangle with bobbing and optional muzzle flash)
    static int frameCounter = 0;
    frameCounter++;
    int gunW = SCREEN_WIDTH / 5;
    int gunH = SCREEN_HEIGHT / 3;
    int bob = (int)(sin(frameCounter * 0.1) * 5);
    int gunX = SCREEN_WIDTH/2 - gunW/2 + (int)(sin(playerAngle) * 4);
    int gunY = SCREEN_HEIGHT - gunH - 10 + bob;
    if (gunX < 0) gunX = 0; if (gunY < 0) gunY = 0;
    if (gunX + gunW > SCREEN_WIDTH) gunW = SCREEN_WIDTH - gunX;
    if (gunY + gunH > SCREEN_HEIGHT) gunH = SCREEN_HEIGHT - gunY;

    uint32_t gunDark = colorref_to_bgra(RGB(40,40,40));
    uint32_t gunLight = colorref_to_bgra(RGB(90,90,90));
    for (int y = 0; y < gunH; ++y) {
        uint32_t *row = backPixels + (gunY + y) * SCREEN_WIDTH + gunX;
        for (int x2 = 0; x2 < gunW; ++x2) {
            // simple vertical gradient
            row[x2] = (y < gunH/3) ? gunLight : gunDark;
        }
    }

    // muzzle flash overlay for a few frames
    static int flashFrames = 0;
    if (flashFrames > 0) {
        flashFrames--;
        int fx = SCREEN_WIDTH/2 - gunW/8;
        int fy = gunY - gunH/6;
        int fw = gunW/4;
        int fh = gunH/6;
        if (fy < 0) fy = 0;
        uint32_t flash = colorref_to_bgra(RGB(255, 240, 160));
        for (int y = 0; y < fh; ++y) {
            int yy = fy + y; if (yy < 0 || yy >= SCREEN_HEIGHT) continue;
            uint32_t *row = backPixels + yy * SCREEN_WIDTH;
            for (int x2 = 0; x2 < fw; ++x2) {
                int xx = fx + x2; if (xx < 0 || xx >= SCREEN_WIDTH) continue;
                row[xx] = flash;
            }
        }
    }
    
    // Render minimap
    renderMinimap(hdc);
}

void renderMinimap(HDC hdc) {
    // Only render minimap if there's enough space
    if (SCREEN_WIDTH < 250 || SCREEN_HEIGHT < 250) {
        return;
    }
    
    int minimapSize = 200;
    int minimapX = SCREEN_WIDTH - minimapSize - 10;
    int minimapY = 10;
    
    // Ensure minimap stays within bounds
    if (minimapX < 10) minimapX = 10;
    if (minimapY < 10) minimapY = 10;
    if (minimapX + minimapSize > SCREEN_WIDTH - 10) {
        minimapX = SCREEN_WIDTH - minimapSize - 10;
    }
    if (minimapY + minimapSize > SCREEN_HEIGHT - 10) {
        minimapY = SCREEN_HEIGHT - minimapSize - 10;
    }
    
    int cellSize = minimapSize / MAP_WIDTH;
    
    // Minimap background with border
    HBRUSH bgBrush = CreateSolidBrush(RGB(0, 0, 0));
    RECT bgRect = {minimapX, minimapY, minimapX + minimapSize, minimapY + minimapSize};
    FillRect(hdc, &bgRect, bgBrush);
    DeleteObject(bgBrush);
    
    // Draw border
    HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    HPEN oldPen = SelectObject(hdc, borderPen);
    MoveToEx(hdc, minimapX, minimapY, NULL);
    LineTo(hdc, minimapX + minimapSize, minimapY);
    LineTo(hdc, minimapX + minimapSize, minimapY + minimapSize);
    LineTo(hdc, minimapX, minimapY + minimapSize);
    LineTo(hdc, minimapX, minimapY);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
    
    // Draw map cells
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (map[y][x] > 0) {
                RECT cell = {
                    minimapX + x * cellSize,
                    minimapY + y * cellSize,
                    minimapX + (x + 1) * cellSize,
                    minimapY + (y + 1) * cellSize
                };
                HBRUSH cellBrush = CreateSolidBrush(wallColors[map[y][x]]);
                FillRect(hdc, &cell, cellBrush);
                DeleteObject(cellBrush);
            }
        }
    }
    
    // Draw player (ensure it's within minimap bounds)
    int playerMapX = minimapX + (int)(playerX * cellSize);
    int playerMapY = minimapY + (int)(playerY * cellSize);
    
    // Clamp player position to minimap bounds
    if (playerMapX < minimapX) playerMapX = minimapX;
    if (playerMapY < minimapY) playerMapY = minimapY;
    if (playerMapX > minimapX + minimapSize) playerMapX = minimapX + minimapSize;
    if (playerMapY > minimapY + minimapSize) playerMapY = minimapY + minimapSize;
    
    HBRUSH playerBrush = CreateSolidBrush(RGB(255, 255, 0)); // Yellow player
    RECT playerRect = {
        playerMapX - 2, playerMapY - 2,
        playerMapX + 2, playerMapY + 2
    };
    FillRect(hdc, &playerRect, playerBrush);
    DeleteObject(playerBrush);
    
    // Draw player direction line
    HPEN directionPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 0));
    oldPen = SelectObject(hdc, directionPen);
    
    int dirX = playerMapX + (int)(cos(playerAngle) * 15);
    int dirY = playerMapY + (int)(sin(playerAngle) * 15);
    
    MoveToEx(hdc, playerMapX, playerMapY, NULL);
    LineTo(hdc, dirX, dirY);
    
    SelectObject(hdc, oldPen);
    DeleteObject(directionPen);

    // Draw all enemies on minimap
    for (int i = 0; i < numEnemies; i++) {
        if (enemies[i].alive) {
            int ex = minimapX + (int)(enemies[i].x * cellSize);
            int ey = minimapY + (int)(enemies[i].y * cellSize);
            HBRUSH enemyBrush = CreateSolidBrush(RGB(255, 0, 0));
            RECT enemyRect = { ex - 2, ey - 2, ex + 2, ey + 2 };
            FillRect(hdc, &enemyRect, enemyBrush);
            DeleteObject(enemyBrush);
        }
    }
}
