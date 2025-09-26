#include "map.h"
#include "texture.h"
#include "player.h"
#include "enemy.h"

// Enhanced map with different wall types (0 = empty, 1-4 = different wall types)
int map[MAP_HEIGHT][MAP_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,2,2,0,0,0,0,0,0,0,0,3,3,0,1},
    {1,0,2,2,0,0,0,0,0,0,0,0,3,3,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,4,4,4,4,4,4,0,0,0,0,1},
    {1,0,0,0,0,4,0,0,0,0,4,0,0,0,0,1},
    {1,0,0,0,0,4,0,0,0,0,4,0,0,0,0,1},
    {1,0,0,0,0,4,0,0,0,0,4,0,0,0,0,1},
    {1,0,0,0,0,4,0,0,0,0,4,0,0,0,0,1},
    {1,0,0,0,0,4,4,0,0,4,4,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,3,3,0,0,0,0,0,0,0,0,2,2,0,1},
    {1,0,3,3,0,0,0,0,0,0,0,0,2,2,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// Texture IDs for each map cell
int mapTextures[MAP_HEIGHT][MAP_WIDTH] = {0};

// Floor texture IDs for each map cell
int mapFloorTextures[MAP_HEIGHT][MAP_WIDTH] = {0};

int loadMapFromFile(const char *path) {
    if (!path) return 0;
    
    // Check file extension to determine format
    char *ext = strrchr(path, '.');
    if (ext && strcmp(ext, ".rwm") == 0) {
        // Load new .rwm format
        FILE *f = fopen(path, "rb");
        if (!f) return 0;
        
        // Read and verify header
        char magic[4] = {0};
        fread(magic, 1, 3, f);
        if (strcmp(magic, RWM_MAGIC) != 0) {
            fclose(f);
            return 0;
        }
        
        int version = fgetc(f);
        if (version != RWM_VERSION) {
            fclose(f);
            return 0;
        }
        
        int mapWidth = fgetc(f) | (fgetc(f) << 8);
        int mapHeight = fgetc(f) | (fgetc(f) << 8);
        
        if (mapWidth != MAP_WIDTH || mapHeight != MAP_HEIGHT) {
            fclose(f);
            return 0;
        }
        
        // Read metadata (skip for now)
        int nameLen = fgetc(f);
        fseek(f, nameLen, SEEK_CUR);
        int descLen = fgetc(f);
        fseek(f, descLen, SEEK_CUR);
        int authorLen = fgetc(f);
        fseek(f, authorLen, SEEK_CUR);
        
        // Reset enemies
        resetEnemies();
        
        // Read map data
        for (int y = 0; y < MAP_HEIGHT; ++y) {
            for (int x = 0; x < MAP_WIDTH; ++x) {
                int wallType = fgetc(f);
                int textureId = fgetc(f);
                int floorTextureId = fgetc(f);
                
                // Clamp values
                if (wallType < 0) wallType = 0;
                if (wallType > 6) wallType = 6;
                if (textureId < 0) textureId = 0;
                if (textureId >= 8) textureId = 0;
                if (floorTextureId < 0) floorTextureId = 0;
                if (floorTextureId >= 8) floorTextureId = 0;
                
                if (wallType == 5) {
                    // Player spawn - set player position
                    setPlayerPosition(x + 0.5, y + 0.5);
                    map[y][x] = 0; // Make it walkable
                } else if (wallType == 6) {
                    // Enemy spawn - add enemy
                    addEnemy(x + 0.5, y + 0.5);
                    map[y][x] = 0; // Make it walkable
                } else {
                    map[y][x] = wallType;
                    mapTextures[y][x] = textureId;
                    mapFloorTextures[y][x] = floorTextureId;
                    // Load textures if not already loaded
                    loadTexture(textureId);
                    loadTexture(floorTextureId);
                }
            }
        }
        
        fclose(f);
        return 1;
    } else {
        // Load old .txt format for backward compatibility
        FILE *f = fopen(path, "r");
        if (!f) return 0;
        int ok = 1;
        
        // Reset enemies
        resetEnemies();
        
        for (int y = 0; y < MAP_HEIGHT && ok; ++y) {
            for (int x = 0; x < MAP_WIDTH && ok; ++x) {
                int wallType = 0;
                int textureId = 0;
                int floorTextureId = 0;
                
                // Try to read wallType:textureId:floorTextureId format first
                if (fscanf(f, "%d:%d:%d", &wallType, &textureId, &floorTextureId) == 3) {
                    // New format with floor texture info
                } else {
                    // Try old format wallType:textureId
                    fseek(f, -1, SEEK_CUR); // Go back one character
                    if (fscanf(f, "%d:%d", &wallType, &textureId) == 2) {
                        floorTextureId = 0; // Default floor texture
                    } else {
                        // Try oldest format (just number)
                        fseek(f, -1, SEEK_CUR); // Go back one character
                        if (fscanf(f, "%d", &wallType) == 1) {
                            textureId = (wallType > 0) ? (wallType - 1) % 8 : 0;
                            floorTextureId = 0; // Default floor texture
                        } else {
                            wallType = 0;
                            textureId = 0;
                            floorTextureId = 0;
                        }
                    }
                }
                
                // Clamp values
                if (wallType < 0) wallType = 0;
                if (wallType > 6) wallType = 6;
                if (textureId < 0) textureId = 0;
                if (textureId >= 8) textureId = 0;
                if (floorTextureId < 0) floorTextureId = 0;
                if (floorTextureId >= 8) floorTextureId = 0;
                
                if (wallType == 5) {
                    // Player spawn - set player position
                    setPlayerPosition(x + 0.5, y + 0.5);
                    map[y][x] = 0; // Make it walkable
                } else if (wallType == 6) {
                    // Enemy spawn - add enemy
                    addEnemy(x + 0.5, y + 0.5);
                    map[y][x] = 0; // Make it walkable
                } else {
                    map[y][x] = wallType;
                    mapTextures[y][x] = textureId;
                    mapFloorTextures[y][x] = floorTextureId;
                    // Load textures if not already loaded
                    loadTexture(textureId);
                    loadTexture(floorTextureId);
                }
            }
        }
        fclose(f);
        return ok;
    }
}

// Collision detection for player movement
int canMoveTo(double newX, double newY) {
    int mapX = (int)newX;
    int mapY = (int)newY;
    
    if (mapX < 0 || mapX >= MAP_WIDTH || mapY < 0 || mapY >= MAP_HEIGHT) {
        return 0;
    }
    
    return map[mapY][mapX] == 0;
}
