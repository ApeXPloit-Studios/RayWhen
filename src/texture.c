#include "texture.h"

// External texture array
Texture textures[MAX_TEXTURES] = {0};
const char* textureFiles[] = {
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
int loadBMPTexture(Texture* tex, const char* filename) {
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
void generateTexture(Texture* tex, const char* filename, int textureId) {
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

void loadTexture(int textureId) {
    if (textureId < 0 || textureId >= MAX_TEXTURES) return;
    if (textures[textureId].loaded) return;
    
    // Try to load BMP first, fallback to procedural if it fails
    if (!loadBMPTexture(&textures[textureId], textureFiles[textureId])) {
        // Fallback to procedural generation
        generateTexture(&textures[textureId], textureFiles[textureId], textureId);
    }
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
