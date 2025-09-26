#ifndef DX11_RENDERER_H
#define DX11_RENDERER_H

#include <windows.h>
#include "raywhen.h"

// DirectX 11 renderer state (simplified for TCC)
typedef struct {
    HMODULE d3d11Module;
    HMODULE dxgiModule;
    
    // Function pointers loaded dynamically
    void* (*D3D11CreateDeviceAndSwapChain)(void*, int, void*, int, void*, int, int, void*, void**, void**, void**, void**);
    void* (*CreateComputeShader)(void*, void*, void*, void*, void**);
    void* (*CreateBuffer)(void*, void*, void*, void**);
    void* (*CreateTexture2D)(void*, void*, void*, void**);
    void* (*CreateShaderResourceView)(void*, void*, void*, void**);
    void* (*CreateUnorderedAccessView)(void*, void*, void*, void**);
    void* (*CSSetShader)(void*, void*, void*, int);
    void* (*CSSetConstantBuffers)(void*, int, int, void**);
    void* (*CSSetShaderResources)(void*, int, int, void**);
    void* (*CSSetUnorderedAccessViews)(void*, int, int, void**, void*);
    void* (*Dispatch)(void*, int, int, int);
    void* (*Map)(void*, void*, int, int, int, void**);
    void* (*Unmap)(void*, void*, int);
    void* (*ClearRenderTargetView)(void*, void*, float*);
    void* (*Present)(void*, int, int);
    
    // COM interfaces (as void* to avoid header dependencies)
    void* device;
    void* context;
    void* swapChain;
    void* renderTargetView;
    void* backBuffer;
    
    // GPU resources
    void* computeShader;
    void* constantBuffer;
    void* outputTexture;
    void* outputUAV;
    void* outputSRV;
    void* mapBuffer;
    void* mapSRV;
    void* textureSRVs[MAX_TEXTURES];
    void* textureSampler;
    
    int initialized;
    HWND hwnd;
} DX11Renderer;

// Renderer type enumeration
typedef enum {
    RENDERER_SOFTWARE = 0,
    RENDERER_DX11 = 1
} RendererType;

// Global renderer state
extern RendererType currentRenderer;
extern DX11Renderer dx11Renderer;

// DirectX 11 renderer functions
int initDX11Renderer(HWND hwnd);
void cleanupDX11Renderer(void);
void renderSceneDX11(HWND hwnd);
void uploadTexturesToGPU(void);
void uploadMapToGPU(void);

// Renderer selection functions
void setRenderer(RendererType type);
RendererType getCurrentRenderer(void);
const char* getRendererName(RendererType type);

#endif // DX11_RENDERER_H
