#ifndef MAP_H
#define MAP_H

#include "raywhen.h"

// RayWhen Map (.rwm) format structure
// Header: "RWM" + version (1 byte) + map width (2 bytes) + map height (2 bytes)
// Metadata: name length (1 byte) + name + description length (1 byte) + description + author length (1 byte) + author
// Map data: wallType (1 byte) + textureId (1 byte) + floorTextureId (1 byte) for each cell

#define RWM_MAGIC "RWM"
#define RWM_VERSION 1
#define RWM_HEADER_SIZE 8  // "RWM" + version + width + height

// Enhanced map with different wall types (0 = empty, 1-4 = different wall types)
extern int map[MAP_HEIGHT][MAP_WIDTH];

// Texture IDs for each map cell
extern int mapTextures[MAP_HEIGHT][MAP_WIDTH];

// Floor texture IDs for each map cell
extern int mapFloorTextures[MAP_HEIGHT][MAP_WIDTH];

// Function declarations
int loadMapFromFile(const char *path);
int canMoveTo(double newX, double newY);

#endif // MAP_H
