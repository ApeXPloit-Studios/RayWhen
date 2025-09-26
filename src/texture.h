#ifndef TEXTURE_H
#define TEXTURE_H

#include "raywhen.h"

// Texture system
typedef struct {
    COLORREF pixels[TEX_WIDTH * TEX_HEIGHT];
    COLORREF pixels_mip[TEX_WIDTH/2 * TEX_HEIGHT/2];  // Half resolution mipmap
    int loaded;
} Texture;

// Function declarations
int loadBMPTexture(Texture* tex, const char* filename);
void generateTexture(Texture* tex, const char* filename, int textureId);
void loadTexture(int textureId);
COLORREF getTextureColor(int wallType, double texX, double texY);

// External texture array
extern Texture textures[MAX_TEXTURES];
extern const char* textureFiles[];

#endif // TEXTURE_H
