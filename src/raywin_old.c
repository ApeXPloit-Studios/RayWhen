#include <windows.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Function declarations
void renderScene(HDC hdc);
void renderMinimap(HDC hdc);
static void shootAtCrosshair(void);

// Configuration
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
#define MOVE_SPEED 0.03
#define ROT_SPEED 0.03
#define MAX_DISTANCE 20.0
#define RAY_STEP 0.01
#define TARGET_FPS 30
#define FRAME_TIME (1000 / TARGET_FPS)  // 33ms for 30 FPS
#define MOUSE_SENS 0.004
#define PITCH_SENS 0.5   // pixels per mouse unit (scaled later)

// Movement physics
#define ACCEL 0.06
#define FRICTION 0.82
#define SLIDE_FRICTION 0.95
#define MAX_SPEED 0.22
#define RUN_MULTIPLIER 1.6

// Global screen dimensions (will be updated on resize)
int SCREEN_WIDTH = DEFAULT_SCREEN_WIDTH;
int SCREEN_HEIGHT = DEFAULT_SCREEN_HEIGHT;

// Back buffer for double buffering
HDC backDC = NULL;
HBITMAP backBMP = NULL;
HBITMAP backOldBMP = NULL;
int backW = 0;
int backH = 0;
uint32_t *backPixels = NULL; // BGRA top-down

// Depth buffer for sprite occlusion (stores corrected distances for each screen column)
double *depthBuffer = NULL;
int depthW = 0;

static inline uint32_t colorref_to_bgra(COLORREF c) {
	return ((uint32_t)GetBValue(c)) | (((uint32_t)GetGValue(c)) << 8) | (((uint32_t)GetRValue(c)) << 16) | 0xFF000000u;
}

static void ensureBackBuffer(HWND hwnd) {
	if (!hwnd) return;
	if (backDC && (backW == SCREEN_WIDTH) && (backH == SCREEN_HEIGHT)) return;

	// Cleanup existing
	if (backDC) {
		if (backOldBMP) SelectObject(backDC, backOldBMP);
		if (backBMP) DeleteObject(backBMP);
		DeleteDC(backDC);
		backDC = NULL;
		backBMP = NULL;
		backOldBMP = NULL;
	}

	HDC wndDC = GetDC(hwnd);
	backDC = CreateCompatibleDC(wndDC);
	BITMAPINFO bmi;
	ZeroMemory(&bmi, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = SCREEN_WIDTH;
	// Negative height for top-down DIB so y=0 is top
	bmi.bmiHeader.biHeight = -SCREEN_HEIGHT;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	void *bits = NULL;
	backBMP = CreateDIBSection(wndDC, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
	backPixels = (uint32_t*)bits;
	backOldBMP = (HBITMAP)SelectObject(backDC, backBMP);
	ReleaseDC(hwnd, wndDC);
	backW = SCREEN_WIDTH;
	backH = SCREEN_HEIGHT;

	// (Re)allocate depth buffer to match current width
	if (depthW != SCREEN_WIDTH) {
		if (depthBuffer) {
			free(depthBuffer);
			depthBuffer = NULL;
		}
		depthBuffer = (double*)malloc(sizeof(double) * SCREEN_WIDTH);
		depthW = SCREEN_WIDTH;
	}
}

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

// Player state
double playerX = 8.5, playerY = 8.5;
double playerAngle = 0.0;
double playerSpeed = MOVE_SPEED;
double rotationSpeed = ROT_SPEED;
double pitchOffset = 0.0; // vertical look in pixels (positive moves horizon down)
int mouseLookEnabled = 0;
double velX = 0.0, velY = 0.0; // velocity for sliding
int frameCounter = 0;
int flashFrames = 0;

// Enemy
typedef struct {
	double x;
	double y;
	double radius;
	int health;
	int alive;
} Enemy;

#define MAX_ENEMIES 10
Enemy enemies[MAX_ENEMIES];
int numEnemies = 0;

// Performance mode: flat-shaded walls (no per-pixel texturing)
int simpleShadingMode = 0;
int perfExplicitlySet = 0; // set to 1 if -perf/--no-perf provided

// Texture system
typedef struct {
    COLORREF pixels[TEX_WIDTH * TEX_HEIGHT];
    COLORREF pixels_mip[TEX_WIDTH/2 * TEX_HEIGHT/2];  // Half resolution mipmap
    int loaded;
} Texture;

static Texture textures[MAX_TEXTURES] = {0};
static const char* textureFiles[] = {
    "assets/Bricks/REDBRICKS.bmp",
    "assets/BuildingTextures/BRICKS.bmp", 
    "assets/Industrial/METALTILE.bmp",
    "assets/Wood/WOODA.bmp",
    "assets/Tech/HIGHTECH.bmp",
    "assets/Rocks/GRAYROCKS.bmp",
    "assets/Bricks/CLAYBRICKS.bmp",
    "assets/Industrial/CROSSWALL.bmp",
    "assets/Urban/GRAYWALL.bmp",
    "assets/Wood/DARKWOOD.bmp",
    "assets/Tech/HEXAGONS.bmp",
    "assets/Rocks/DIRT.bmp",
    "assets/Bricks/BIGBRICKS.bmp",
    "assets/Industrial/STORAGE.bmp",
    "assets/Urban/PAVEMENT.bmp",
    "assets/Wood/WOODTILE.bmp"
};

// Direct BMP file reader with debug output
static int loadBMPTexture(Texture* tex, const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return 0; // File not found
    }
    
    // Read BMP header
    unsigned char header[54];
    if (fread(header, 1, 54, file) != 54) {
        fclose(file);
        return 0;
    }
    
    // Check BMP signature
    if (header[0] != 'B' || header[1] != 'M') {
        fclose(file);
        return 0;
    }
    
    // Get image dimensions
    int width = *(int*)&header[18];
    int height = *(int*)&header[22];
    int bitsPerPixel = *(short*)&header[28];
    
    
    // Support 8-bit, 16-bit, 24-bit, and 32-bit BMPs
    if (bitsPerPixel != 8 && bitsPerPixel != 16 && bitsPerPixel != 24 && bitsPerPixel != 32) {
        fclose(file);
        return 0;
    }
    
    // For 8-bit BMPs, we need to read the color palette
    unsigned char palette[256][4] = {0};
    if (bitsPerPixel == 8) {
        int colorsUsed = *(int*)&header[46];
        if (colorsUsed == 0) colorsUsed = 256;
        
        // Read palette
        for (int i = 0; i < colorsUsed; i++) {
            if (fread(palette[i], 1, 4, file) != 4) {
                fclose(file);
                return 0;
            }
        }
    }
    
    // Calculate row padding
    int bytesPerPixel = bitsPerPixel / 8;
    int rowPadding = (4 - (width * bytesPerPixel) % 4) % 4;
    
    // Seek to pixel data
    int dataOffset = *(int*)&header[10];
    fseek(file, dataOffset, SEEK_SET);
    
    // Read pixel data and resize to our texture size
    for (int y = 0; y < TEX_HEIGHT; y++) {
        for (int x = 0; x < TEX_WIDTH; x++) {
            // Calculate source coordinates
            int srcX = (x * width) / TEX_WIDTH;
            int srcY = (y * height) / TEX_HEIGHT;
            
            // Clamp to valid range
            if (srcX < 0) srcX = 0;
            if (srcX >= width) srcX = width - 1;
            if (srcY < 0) srcY = 0;
            if (srcY >= height) srcY = height - 1;
            
            // BMPs are stored bottom-up, so flip Y coordinate
            int actualSrcY = height - 1 - srcY;
            
            // Calculate file position for this pixel
            long pixelPos = dataOffset + actualSrcY * (width * bytesPerPixel + rowPadding) + srcX * bytesPerPixel;
            fseek(file, pixelPos, SEEK_SET);
            
            COLORREF pixelColor;
            
            if (bitsPerPixel == 8) {
                // 8-bit: read palette index
                unsigned char paletteIndex;
                if (fread(&paletteIndex, 1, 1, file) == 1) {
                    unsigned char b = palette[paletteIndex][0];
                    unsigned char g = palette[paletteIndex][1];
                    unsigned char r = palette[paletteIndex][2];
                    pixelColor = RGB(r, g, b);
                } else {
                    pixelColor = RGB(128, 128, 128);
                }
            } else if (bitsPerPixel == 24) {
                // 24-bit: read BGR pixel data
                unsigned char bgr[3];
                if (fread(bgr, 1, 3, file) == 3) {
                    // Convert BGR to RGB
                    unsigned char r = bgr[2];
                    unsigned char g = bgr[1];
                    unsigned char b = bgr[0];
                    pixelColor = RGB(r, g, b);
                } else {
                    pixelColor = RGB(128, 128, 128);
                }
            } else if (bitsPerPixel == 32) {
                // 32-bit: read BGRA pixel data
                unsigned char bgra[4];
                if (fread(bgra, 1, 4, file) == 4) {
                    // Convert BGRA to RGB (ignore alpha)
                    unsigned char r = bgra[2];
                    unsigned char g = bgra[1];
                    unsigned char b = bgra[0];
                    pixelColor = RGB(r, g, b);
                } else {
                    pixelColor = RGB(128, 128, 128);
                }
            } else {
                // 16-bit: simplified handling
                unsigned short pixel16;
                if (fread(&pixel16, 1, 2, file) == 2) {
                    // Extract RGB from 16-bit (5-6-5 format)
                    unsigned char r = (pixel16 >> 11) & 0x1F;
                    unsigned char g = (pixel16 >> 5) & 0x3F;
                    unsigned char b = pixel16 & 0x1F;
                    // Scale to 8-bit
                    r = (r * 255) / 31;
                    g = (g * 255) / 63;
                    b = (b * 255) / 31;
                    pixelColor = RGB(r, g, b);
                } else {
                    pixelColor = RGB(128, 128, 128);
                }
            }
            
            tex->pixels[y * TEX_WIDTH + x] = pixelColor;
        }
    }
    
    fclose(file);
    
    // Generate mipmap (half resolution)
    for (int y = 0; y < TEX_HEIGHT/2; y++) {
        for (int x = 0; x < TEX_WIDTH/2; x++) {
            // Sample 2x2 block from original texture
            COLORREF c00 = tex->pixels[(y*2) * TEX_WIDTH + (x*2)];
            COLORREF c01 = tex->pixels[(y*2) * TEX_WIDTH + (x*2+1)];
            COLORREF c10 = tex->pixels[(y*2+1) * TEX_WIDTH + (x*2)];
            COLORREF c11 = tex->pixels[(y*2+1) * TEX_WIDTH + (x*2+1)];
            
            // Average the 4 pixels
            int r = (GetRValue(c00) + GetRValue(c01) + GetRValue(c10) + GetRValue(c11)) / 4;
            int g = (GetGValue(c00) + GetGValue(c01) + GetGValue(c10) + GetGValue(c11)) / 4;
            int b = (GetBValue(c00) + GetBValue(c01) + GetBValue(c10) + GetBValue(c11)) / 4;
            
            tex->pixels_mip[y * (TEX_WIDTH/2) + x] = RGB(r, g, b);
        }
    }
    
    tex->loaded = 1;
    return 1;
}

// Generate procedural texture based on filename hash
static void generateTexture(Texture* tex, const char* filename, int textureId) {
    // Simple hash of filename for deterministic patterns
    unsigned int hash = 0;
    for (int i = 0; filename[i]; i++) {
        hash = hash * 31 + filename[i];
    }
    
    for (int y = 0; y < TEX_HEIGHT; y++) {
        for (int x = 0; x < TEX_WIDTH; x++) {
            COLORREF color;
            
            // Generate different patterns based on texture type
            switch (textureId) {
                case 0: // Red bricks
                    color = ((x/8 + y/8) % 2) ? RGB(180,80,60) : RGB(160,60,40);
                    break;
                case 1: // Gray bricks  
                    color = ((x/6 + y/6) % 2) ? RGB(120,120,120) : RGB(100,100,100);
                    break;
                case 2: // Metal
                    color = RGB(140,140,150) + ((x^y) % 20);
                    break;
                case 3: // Wood
                    color = RGB(139,90,43) + ((y/4) % 15);
                    break;
                case 4: // Tech
                    color = ((x/4 + y/4) % 2) ? RGB(60,100,140) : RGB(40,80,120);
                    break;
                case 5: // Rocks
                    color = RGB(100,100,95) + ((x^y^hash) % 25);
                    break;
                case 6: // Clay bricks
                    color = ((x/8 + y/8) % 2) ? RGB(160,100,80) : RGB(140,80,60);
                    break;
                case 7: // Cross wall
                    color = ((x/4 + y/4) % 2) ? RGB(120,120,110) : RGB(100,100,90);
                    break;
                case 8: // Gray wall
                    color = RGB(120,120,120) + ((x^y) % 10);
                    break;
                case 9: // Dark wood
                    color = RGB(100,70,35) + ((y/3) % 12);
                    break;
                case 10: // Hexagons
                    color = ((x/6 + y/6) % 2) ? RGB(80,120,160) : RGB(60,100,140);
                    break;
                case 11: // Dirt
                    color = RGB(120,100,80) + ((x^y^hash) % 20);
                    break;
                case 12: // Big bricks
                    color = ((x/12 + y/12) % 2) ? RGB(170,90,70) : RGB(150,70,50);
                    break;
                case 13: // Storage
                    color = RGB(110,110,120) + ((x/8 + y/8) % 15);
                    break;
                case 14: // Pavement
                    color = RGB(100,100,100) + ((x^y) % 8);
                    break;
                case 15: // Wood tile
                    color = RGB(150,120,80) + ((x/4 + y/4) % 10);
                    break;
                default:
                    color = RGB(128,128,128);
            }
            
            tex->pixels[y * TEX_WIDTH + x] = color;
        }
    }
    tex->loaded = 1;
}

static void loadTexture(int textureId) {
    if (textureId < 0 || textureId >= MAX_TEXTURES) return;
    if (textures[textureId].loaded) return;
    
    // Try to load BMP first, fallback to procedural if it fails
    if (!loadBMPTexture(&textures[textureId], textureFiles[textureId])) {
        // Fallback to procedural generation
        generateTexture(&textures[textureId], textureFiles[textureId], textureId);
    }
}

// RayWhen Map (.rwm) format structure
// Header: "RWM" + version (1 byte) + map width (2 bytes) + map height (2 bytes)
// Metadata: name length (1 byte) + name + description length (1 byte) + description + author length (1 byte) + author
// Map data: wallType (1 byte) + textureId (1 byte) + floorTextureId (1 byte) for each cell

#define RWM_MAGIC "RWM"
#define RWM_VERSION 1
#define RWM_HEADER_SIZE 8  // "RWM" + version + width + height

static int loadMapFromFile(const char *path) {
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
        numEnemies = 0;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            enemies[i].alive = 0;
        }
        
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
                    playerX = x + 0.5;
                    playerY = y + 0.5;
                    map[y][x] = 0; // Make it walkable
                } else if (wallType == 6) {
                    // Enemy spawn - add enemy
                    if (numEnemies < MAX_ENEMIES) {
                        enemies[numEnemies].x = x + 0.5;
                        enemies[numEnemies].y = y + 0.5;
                        enemies[numEnemies].radius = 0.3;
                        enemies[numEnemies].health = 1;
                        enemies[numEnemies].alive = 1;
                        numEnemies++;
                    }
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
        numEnemies = 0;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            enemies[i].alive = 0;
        }
        
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
                    playerX = x + 0.5;
                    playerY = y + 0.5;
                    map[y][x] = 0; // Make it walkable
                } else if (wallType == 6) {
                    // Enemy spawn - add enemy
                    if (numEnemies < MAX_ENEMIES) {
                        enemies[numEnemies].x = x + 0.5;
                        enemies[numEnemies].y = y + 0.5;
                        enemies[numEnemies].radius = 0.3;
                        enemies[numEnemies].health = 1;
                        enemies[numEnemies].alive = 1;
                        numEnemies++;
                    }
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

// Command-line parsing for launcher options
static void parseLaunchArgs(void) {
    char *cmd = GetCommandLineA();
    if (!cmd) return;
    // Simple space-delimited parse; our args do not require quotes
    // Duplicate string since strtok modifies it
    size_t len = strlen(cmd);
    char *buf = (char*)malloc(len + 1);
    if (!buf) return;
    memcpy(buf, cmd, len + 1);
    char *tok = strtok(buf, " \t\r\n");
    // skip program name
    if (tok) tok = strtok(NULL, " \t\r\n");
    while (tok) {
        if (strcmp(tok, "-mouselook") == 0 || strcmp(tok, "--mouselook") == 0) {
            mouseLookEnabled = 1;
            tok = strtok(NULL, " \t\r\n");
            continue;
        }
        if (strcmp(tok, "-perf") == 0 || strcmp(tok, "--performance") == 0) {
            simpleShadingMode = 1;
            perfExplicitlySet = 1;
            tok = strtok(NULL, " \t\r\n");
            continue;
        }
        if (strcmp(tok, "--no-perf") == 0) {
            simpleShadingMode = 0;
            perfExplicitlySet = 1;
            tok = strtok(NULL, " \t\r\n");
            continue;
        }
        if (strcmp(tok, "-map") == 0) {
            char *next = strtok(NULL, " \t\r\n");
            if (next) {
                loadMapFromFile(next);
            }
            tok = strtok(NULL, " \t\r\n");
            continue;
        }
        if ((strcmp(tok, "-w") == 0 || strcmp(tok, "--width") == 0)) {
            char *next = strtok(NULL, " \t\r\n");
            if (next) {
                int w = atoi(next);
                if (w >= MIN_SCREEN_WIDTH && w <= 4096) SCREEN_WIDTH = w;
            }
            tok = strtok(NULL, " \t\r\n");
            continue;
        }
        if ((strcmp(tok, "-h") == 0 || strcmp(tok, "--height") == 0)) {
            char *next = strtok(NULL, " \t\r\n");
            if (next) {
                int h = atoi(next);
                if (h >= MIN_SCREEN_HEIGHT && h <= 2160) SCREEN_HEIGHT = h;
            }
            tok = strtok(NULL, " \t\r\n");
            continue;
        }
        tok = strtok(NULL, " \t\r\n");
    }
    free(buf);
}

// Wall colors for different types
COLORREF wallColors[] = {
    RGB(0, 0, 0),       // 0 - empty (shouldn't be used)
    RGB(120, 120, 120), // 1 - gray
    RGB(180, 100, 100), // 2 - red
    RGB(100, 100, 180), // 3 - blue
    RGB(100, 180, 100)  // 4 - green
};

// Simple procedural texture function
COLORREF getTextureColor(int wallType, double texX, double texY) {
    COLORREF baseColor = wallColors[wallType];
    int r = GetRValue(baseColor);
    int g = GetGValue(baseColor);
    int b = GetBValue(baseColor);
    
    // Create a simple brick pattern
    int brickX = (int)(texX * 8) % 2;
    int brickY = (int)(texY * 4) % 2;
    
    // Add some variation based on position
    int variation = (int)(texX * 31 + texY * 17) % 40;
    
    if (wallType == 1) {
        // Gray bricks
        if (brickX == 0 && brickY == 0) {
            r += variation - 20;
            g += variation - 20;
            b += variation - 20;
        }
    } else if (wallType == 2) {
        // Red bricks with mortar
        if (brickX == 1 || brickY == 1) {
            r = 80; g = 80; b = 80; // Mortar
        } else {
            r += variation - 20;
        }
    } else if (wallType == 3) {
        // Blue stone pattern
        if ((brickX + brickY) % 2 == 0) {
            r += variation - 15;
            g += variation - 15;
            b += variation - 15;
        }
    } else if (wallType == 4) {
        // Green mossy pattern
        if (brickX == 0) {
            r += variation - 25;
            g += variation - 10;
            b += variation - 25;
        }
    }
    
    // Clamp values
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    
    return RGB(r, g, b);
}

// Improved DDA raycasting algorithm
typedef struct {
    double distance;
    int wallType;
    int side; // 0 for horizontal walls, 1 for vertical walls
    double wallX; // Where on the wall the ray hit (for texture mapping)
} RayResult;

RayResult castRay(double angle) {
    double sinA = sin(angle);
    double cosA = cos(angle);
    
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

// Collision detection for player movement
int canMoveTo(double newX, double newY) {
    int mapX = (int)newX;
    int mapY = (int)newY;
    
    if (mapX < 0 || mapX >= MAP_WIDTH || mapY < 0 || mapY >= MAP_HEIGHT) {
        return 0;
    }
    
    return map[mapY][mapX] == 0;
}

// Simple hitscan at crosshair against all enemies
static void shootAtCrosshair(void) {
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

void renderScene(HDC hdc) {
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

    // HUD gun (simple rectangle with bobbing and optional muzzle flash)
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

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static int keys[256] = {0};
    static double lastPlayerX = 8.5, lastPlayerY = 8.5, lastPlayerAngle = 0.0;
    
    switch (msg) {
        case WM_SIZE: {
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            SCREEN_WIDTH = clientRect.right - clientRect.left;
            SCREEN_HEIGHT = clientRect.bottom - clientRect.top;
            
            // Enforce minimum size
            if (SCREEN_WIDTH < MIN_SCREEN_WIDTH) SCREEN_WIDTH = MIN_SCREEN_WIDTH;
            if (SCREEN_HEIGHT < MIN_SCREEN_HEIGHT) SCREEN_HEIGHT = MIN_SCREEN_HEIGHT;
            
            ensureBackBuffer(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        }
        
        case WM_KEYDOWN:
            keys[wParam] = 1;
            if (wParam == VK_ESCAPE) {
                PostQuitMessage(0);
            }
            if (wParam == 'M') {
                mouseLookEnabled = !mouseLookEnabled;
                ShowCursor(!mouseLookEnabled);
                if (mouseLookEnabled) {
                    // Recenter cursor
                    RECT rc; GetClientRect(hwnd, &rc);
                    POINT pt = { (rc.right-rc.left)/2, (rc.bottom-rc.top)/2 };
                    ClientToScreen(hwnd, &pt);
                    SetCursorPos(pt.x, pt.y);
                }
            }
            break;

        case WM_LBUTTONDOWN:
            flashFrames = 6; // short muzzle flash
            shootAtCrosshair();
            InvalidateRect(hwnd, NULL, FALSE);
            break;
            
        case WM_KEYUP:
            keys[wParam] = 0;
            break;
            
        case WM_TIMER: {
            double oldX = playerX, oldY = playerY, oldAngle = playerAngle;
            
            // Handle continuous input
            if (keys[VK_LEFT]) {
                playerAngle -= rotationSpeed;
            }
            if (keys[VK_RIGHT]) {
                playerAngle += rotationSpeed;
            }
            // Acceleration-based movement (supports sliding)
            double accel = ACCEL * (keys[VK_SHIFT] ? RUN_MULTIPLIER : 1.0);
            if (keys[VK_UP] || keys['W']) {
                velX += cos(playerAngle) * accel;
                velY += sin(playerAngle) * accel;
            }
            if (keys[VK_DOWN] || keys['S']) {
                velX -= cos(playerAngle) * accel;
                velY -= sin(playerAngle) * accel;
            }
            if (keys['A']) {
                velX += cos(playerAngle - M_PI/2) * accel;
                velY += sin(playerAngle - M_PI/2) * accel;
            }
            if (keys['D']) {
                velX += cos(playerAngle + M_PI/2) * accel;
                velY += sin(playerAngle + M_PI/2) * accel;
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
            
            // Only redraw if something actually changed or a flash is active
            if (playerX != oldX || playerY != oldY || playerAngle != oldAngle || flashFrames > 0) {
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        }

        case WM_MOUSEMOVE: {
            if (mouseLookEnabled) {
                RECT rc; GetClientRect(hwnd, &rc);
                int cx = (rc.right-rc.left)/2;
                int cy = (rc.bottom-rc.top)/2;
                POINT pt; GetCursorPos(&pt);
                POINT winPt = pt; ScreenToClient(hwnd, &winPt);
                int dx = winPt.x - cx;
                int dy = winPt.y - cy;
                if (dx != 0 || dy != 0) {
                    playerAngle += dx * MOUSE_SENS;
                    pitchOffset += -(double)dy * PITCH_SENS;
                    // clamp pitch offset to avoid excessive tilt
                    double maxPitch = SCREEN_HEIGHT * 0.45;
                    if (pitchOffset < -maxPitch) pitchOffset = -maxPitch;
                    if (pitchOffset >  maxPitch) pitchOffset =  maxPitch;
                    InvalidateRect(hwnd, NULL, FALSE);
                    // recenter cursor
                    POINT cpt = { cx, cy }; ClientToScreen(hwnd, &cpt);
                    SetCursorPos(cpt.x, cpt.y);
                }
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            ensureBackBuffer(hwnd);
            // Draw into back buffer
            renderScene(backDC);
            // Blit to screen
            BitBlt(hdc, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, backDC, 0, 0, SRCCOPY);
            EndPaint(hwnd, &ps);
        } break;

        case WM_DESTROY:
            if (depthBuffer) { free(depthBuffer); depthBuffer = NULL; depthW = 0; }
            PostQuitMessage(0);
            return 0;
        
        case WM_ERASEBKGND:
            // Prevent flicker; we fully redraw each frame
            return 1;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    parseLaunchArgs();
    const char g_szClassName[] = "RaycasterWinClass";

    WNDCLASS wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = g_szClassName;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // Calculate window size including borders
    RECT windowRect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
    
    HWND hwnd = CreateWindowEx(
        0,
        g_szClassName,
        simpleShadingMode ? "Advanced Raycasting Engine [PERF] - WASD/Arrows to move, Shift to run" : "Advanced Raycasting Engine - WASD/Arrows to move, Shift to run",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) {
        MessageBox(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ensureBackBuffer(hwnd);

    // Auto-enable perf mode at high res only if not explicitly set via args
    if (!perfExplicitlySet && (SCREEN_WIDTH * SCREEN_HEIGHT >= 1920 * 1080)) {
        simpleShadingMode = 1;
        SetWindowText(hwnd, "Advanced Raycasting Engine [PERF] - WASD/Arrows to move, Shift to run");
    }

    // Set up timer for smooth input handling (30 FPS)
    SetTimer(hwnd, 1, FRAME_TIME, NULL); // 30 FPS
    
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    KillTimer(hwnd, 1);
    return msg.wParam;
}
