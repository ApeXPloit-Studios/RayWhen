#include <windows.h>
#include <stdio.h>

// Common Dialog structures and constants (since commdlg.h is not available in TCC)
typedef UINT_PTR (CALLBACK* LPOFNHOOKPROC)(HWND, UINT, WPARAM, LPARAM);

typedef struct tagOFNA {
    DWORD lStructSize;
    HWND hwndOwner;
    HINSTANCE hInstance;
    LPCSTR lpstrFilter;
    LPSTR lpstrCustomFilter;
    DWORD nMaxCustFilter;
    DWORD nFilterIndex;
    LPSTR lpstrFile;
    DWORD nMaxFile;
    LPSTR lpstrFileTitle;
    DWORD nMaxFileTitle;
    LPCSTR lpstrInitialDir;
    LPCSTR lpstrTitle;
    DWORD Flags;
    WORD nFileOffset;
    WORD nFileExtension;
    LPCSTR lpstrDefExt;
    LPARAM lCustData;
    LPOFNHOOKPROC lpfnHook;
    LPCSTR lpTemplateName;
} OPENFILENAMEA, *LPOPENFILENAMEA;

// Common Dialog flags
#define OFN_READONLY 0x00000001
#define OFN_OVERWRITEPROMPT 0x00000002
#define OFN_HIDEREADONLY 0x00000004
#define OFN_NOCHANGEDIR 0x00000008
#define OFN_SHOWHELP 0x00000010
#define OFN_ENABLEHOOK 0x00000020
#define OFN_ENABLETEMPLATE 0x00000040
#define OFN_ENABLETEMPLATEHANDLE 0x00000080
#define OFN_NOVALIDATE 0x00000100
#define OFN_ALLOWMULTISELECT 0x00000200
#define OFN_EXTENSIONDIFFERENT 0x00000400
#define OFN_PATHMUSTEXIST 0x00000800
#define OFN_FILEMUSTEXIST 0x00001000
#define OFN_CREATEPROMPT 0x00002000
#define OFN_SHAREAWARE 0x00004000
#define OFN_NOREADONLYRETURN 0x00008000
#define OFN_NOTESTFILECREATE 0x00010000
#define OFN_NONETWORKBUTTON 0x00020000
#define OFN_NOLONGNAMES 0x00040000
#define OFN_EXPLORER 0x00080000
#define OFN_NODEREFERENCELINKS 0x00100000
#define OFN_LONGNAMES 0x00200000
#define OFN_ENABLEINCLUDENOTIFY 0x00400000
#define OFN_ENABLESIZING 0x00800000
#define OFN_DONTADDTORECENT 0x02000000
#define OFN_FORCESHOWHIDDEN 0x10000000

// Function declarations
BOOL WINAPI GetOpenFileNameA(LPOPENFILENAMEA lpofn);
BOOL WINAPI GetSaveFileNameA(LPOPENFILENAMEA lpofn);

#define MAP_W 16
#define MAP_H 16
#define MAX_TEXTURES 8

typedef struct {
    int wallType;
    int textureId;
    int floorTextureId;
} MapCell;

static MapCell mapData[MAP_H][MAP_W] = {0};
static int currentTile = 1; // 0 empty, 1..4 walls, 5 player spawn, 6 enemy spawn
static int currentTexture = 0; // 0..MAX_TEXTURES-1
static int currentFloorTexture = 0; // 0..MAX_TEXTURES-1
static char currentFile[MAX_PATH] = "maps\\map.txt";
static char currentFileName[MAX_PATH] = "map.txt";
static BOOL needsRedraw = FALSE;

// Texture names for dropdown
static const char* textureNames[] = {
    "Brick Red", "Stone Gray", "Metal Silver", "Wood Brown", 
    "Tech Blue", "Rock Dark", "Brick Clay", "Metal Tile"
};

// Floor texture names (matching game engine texture files)
static const char* floorTextureNames[] = {
    "Red Bricks", "Building Bricks", "Metal Tile", "Wood A", 
    "High Tech", "Gray Rocks", "Clay Bricks", "Cross Wall"
};

static void saveMap(const char *path) {
    // Clean the filename by replacing spaces with underscores
    char cleanPath[MAX_PATH];
    strcpy(cleanPath, path);
    for (int i = 0; cleanPath[i]; i++) {
        if (cleanPath[i] == ' ') {
            cleanPath[i] = '_';
        }
    }
    
    FILE *f = fopen(cleanPath, "w");
    if (!f) {
        // Try to show error message for debugging
        char errorMsg[512];
        wsprintfA(errorMsg, "Failed to save file: %s", cleanPath);
        MessageBoxA(NULL, errorMsg, "Save Error", MB_OK | MB_ICONERROR);
        return;
    }
    for (int y = 0; y < MAP_H; ++y) {
        for (int x = 0; x < MAP_W; ++x) {
            fprintf(f, "%d:%d:%d ", mapData[y][x].wallType, mapData[y][x].textureId, mapData[y][x].floorTextureId);
        }
        fprintf(f, "\n");
    }
    fclose(f);
}


static BOOL showSaveDialog(HWND hwnd, char *filename) {
    OPENFILENAMEA ofn = {0};
    char fileBuffer[MAX_PATH] = {0};
    
    // Initialize the filename with current filename (without extension)
    char baseName[MAX_PATH] = {0};
    strcpy(baseName, currentFileName);
    char *ext = strstr(baseName, ".txt");
    if (ext) *ext = '\0';
    strcpy(fileBuffer, baseName);
    
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "Map Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = "Save Map As";
    ofn.lpstrDefExt = "txt";
    ofn.lpstrInitialDir = "maps";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    
    if (GetSaveFileNameA(&ofn)) {
        strcpy(filename, fileBuffer);
        return TRUE;
    }
    return FALSE;
}

static BOOL showLoadDialog(HWND hwnd, char *filename) {
    OPENFILENAMEA ofn = {0};
    char fileBuffer[MAX_PATH] = {0};
    
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "Map Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = "Load Map";
    ofn.lpstrInitialDir = "maps";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    
    if (GetOpenFileNameA(&ofn)) {
        strcpy(filename, fileBuffer);
        return TRUE;
    }
    return FALSE;
}

static void loadMap(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        // Try to show error message for debugging
        char errorMsg[512];
        wsprintfA(errorMsg, "Failed to open file: %s", path);
        MessageBoxA(NULL, errorMsg, "Load Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    for (int y = 0; y < MAP_H; ++y) {
        for (int x = 0; x < MAP_W; ++x) {
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
                        textureId = (wallType > 0) ? (wallType - 1) % MAX_TEXTURES : 0;
                        floorTextureId = 0; // Default floor texture
                    }
                }
            }
            
            // Clamp values
            if (wallType < 0) wallType = 0;
            if (wallType > 6) wallType = 6;
            if (textureId < 0) textureId = 0;
            if (textureId >= MAX_TEXTURES) textureId = 0;
            if (floorTextureId < 0) floorTextureId = 0;
            if (floorTextureId >= MAX_TEXTURES) floorTextureId = 0;
            
            mapData[y][x].wallType = wallType;
            mapData[y][x].textureId = textureId;
            mapData[y][x].floorTextureId = floorTextureId;
        }
    }
    fclose(f);
}

static COLORREF tileColor(int wallType, int textureId, int floorTextureId) {
    switch (wallType) {
        case 1: 
            // Different shades of red based on texture
            switch (textureId % 4) {
                case 0: return RGB(180,100,80);  // Brick Red
                case 1: return RGB(160,80,70);   // Darker Red
                case 2: return RGB(200,120,90);  // Lighter Red
                case 3: return RGB(140,60,50);   // Dark Red
                default: return RGB(180,100,80);
            }
        case 2: 
            // Different shades of green based on texture
            switch (textureId % 4) {
                case 0: return RGB(100,180,100); // Green
                case 1: return RGB(80,160,80);   // Darker Green
                case 2: return RGB(120,200,120); // Lighter Green
                case 3: return RGB(60,140,60);   // Dark Green
                default: return RGB(100,180,100);
            }
        case 3: 
            // Different shades of blue based on texture
            switch (textureId % 4) {
                case 0: return RGB(100,100,180); // Blue
                case 1: return RGB(80,80,160);   // Darker Blue
                case 2: return RGB(120,120,200); // Lighter Blue
                case 3: return RGB(60,60,140);   // Dark Blue
                default: return RGB(100,100,180);
            }
        case 4: 
            // Different shades of yellow based on texture
            switch (textureId % 4) {
                case 0: return RGB(180,180,100); // Yellow
                case 1: return RGB(160,160,80);  // Darker Yellow
                case 2: return RGB(200,200,120); // Lighter Yellow
                case 3: return RGB(140,140,60);  // Dark Yellow
                default: return RGB(180,180,100);
            }
        case 5: return RGB(255,255,0);   // Player spawn (bright yellow)
        case 6: return RGB(255,0,0);     // Enemy spawn (red)
        case 0: 
            // Empty tile - show floor texture color
            switch (floorTextureId % 8) {
                case 0: return RGB(180,80,60);   // Red Bricks
                case 1: return RGB(160,80,70);  // Building Bricks
                case 2: return RGB(169,169,169); // Metal Tile
                case 3: return RGB(139,69,19);   // Wood A
                case 4: return RGB(100,100,180); // High Tech
                case 5: return RGB(120,120,120); // Gray Rocks
                case 6: return RGB(140,60,50);   // Clay Bricks
                case 7: return RGB(80,80,160);  // Cross Wall
                default: return RGB(120,120,120); // Default
            }
        default: return RGB(20,20,20);   // Fallback (dark)
    }
}

#define IDC_TEXTURE_COMBO 1001
#define IDC_FLOOR_TEXTURE_COMBO 1002

static HWND hTextureCombo = NULL;
static HWND hFloorTextureCombo = NULL;
static HWND hWallTextureLabel = NULL;
static HWND hFloorTextureLabel = NULL;

static void calculateGridLayout(HWND hwnd, int *ox, int *oy, int *cell, int *size) {
    RECT rc; GetClientRect(hwnd, &rc);
    
    // Calculate available space, leaving room for legend and controls at bottom
    int availableWidth = rc.right - rc.left;
    int availableHeight = rc.bottom - rc.top - 50; // Leave space for legend
    
    // Use the smaller dimension to maintain square aspect ratio, but cap at reasonable size
    *size = min(availableWidth, availableHeight) - 20;
    if (*size < 16) *size = 16;
    if (*size > 500) *size = 500; // Cap maximum size to prevent huge grids
    
    // Calculate cell size and ensure it's at least 1 pixel
    *cell = *size / MAP_W; 
    if (*cell < 1) *cell = 1; 
    
    // Recalculate size to be exact multiple of cell size
    *size = *cell * MAP_W;
    
    // Center the grid horizontally in the full window width
    *ox = (rc.right - rc.left - *size) / 2;
    *oy = 50; // Move grid down to clear the dropdowns
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE: {
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            int clientWidth = clientRect.right - clientRect.left;
            int clientHeight = clientRect.bottom - clientRect.top;
            
            // Center the controls horizontally
            int centerX = clientWidth / 2;
            int controlY = 10;
            
            // Wall texture controls
            SetWindowPos(hWallTextureLabel, NULL, centerX - 250, controlY, 80, 20, SWP_NOZORDER);
            SetWindowPos(hTextureCombo, NULL, centerX - 155, controlY - 2, 150, 200, SWP_NOZORDER);
            
            // Floor texture controls
            SetWindowPos(hFloorTextureLabel, NULL, centerX + 20, controlY, 80, 20, SWP_NOZORDER);
            SetWindowPos(hFloorTextureCombo, NULL, centerX + 105, controlY - 2, 150, 200, SWP_NOZORDER);
            
            // Clear window and redraw (this will also redraw the status text at the bottom)
            InvalidateRect(hwnd, NULL, TRUE); // TRUE = erase background
            UpdateWindow(hwnd); // Force immediate redraw
            break;
        }
        case WM_CREATE: {
            // Create wall texture dropdown
            hWallTextureLabel = CreateWindowA("STATIC", "Wall Texture:", WS_VISIBLE|WS_CHILD, 10, 10, 80, 20, hwnd, NULL, NULL, NULL);
            hTextureCombo = CreateWindowA("COMBOBOX", "", WS_VISIBLE|WS_CHILD|CBS_DROPDOWNLIST|CBS_AUTOHSCROLL, 95, 8, 150, 200, hwnd, (HMENU)IDC_TEXTURE_COMBO, NULL, NULL);
            
            // Add wall texture items to dropdown
            for (int i = 0; i < 8; i++) {
                SendMessageA(hTextureCombo, CB_ADDSTRING, 0, (LPARAM)textureNames[i]);
            }
            SendMessageA(hTextureCombo, CB_SETCURSEL, 0, 0); // Select first item
            
            // Create floor texture dropdown
            hFloorTextureLabel = CreateWindowA("STATIC", "Floor Texture:", WS_VISIBLE|WS_CHILD, 260, 10, 80, 20, hwnd, NULL, NULL, NULL);
            hFloorTextureCombo = CreateWindowA("COMBOBOX", "", WS_VISIBLE|WS_CHILD|CBS_DROPDOWNLIST|CBS_AUTOHSCROLL, 345, 8, 150, 200, hwnd, (HMENU)IDC_FLOOR_TEXTURE_COMBO, NULL, NULL);
            
            // Add floor texture items to dropdown
            for (int i = 0; i < 8; i++) {
                SendMessageA(hFloorTextureCombo, CB_ADDSTRING, 0, (LPARAM)floorTextureNames[i]);
            }
            SendMessageA(hFloorTextureCombo, CB_SETCURSEL, 0, 0); // Select first item
            
            // Start timer for batched redraws (every 16ms = ~60fps)
            SetTimer(hwnd, 1, 16, NULL);
            
            loadMap(currentFile);
            break;
        }
        case WM_TIMER: {
            if (needsRedraw) {
                needsRedraw = FALSE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        }
        case WM_LBUTTONDOWN:
        case WM_MOUSEMOVE: {
            if ((msg == WM_MOUSEMOVE) && !(wParam & MK_LBUTTON)) break;
            
            int ox, oy, cell, size;
            calculateGridLayout(hwnd, &ox, &oy, &cell, &size);
            
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);
            int gx = (mx - ox) / cell;
            int gy = (my - oy) / cell;
            if (gx >= 0 && gx < MAP_W && gy >= 0 && gy < MAP_H) {
                // Only update if the tile actually changes
                if (mapData[gy][gx].wallType != currentTile || 
                    mapData[gy][gx].textureId != currentTexture || 
                    mapData[gy][gx].floorTextureId != currentFloorTexture) {
                    mapData[gy][gx].wallType = currentTile;
                    mapData[gy][gx].textureId = currentTexture;
                    mapData[gy][gx].floorTextureId = currentFloorTexture;
                    needsRedraw = TRUE;
                }
            }
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == IDC_TEXTURE_COMBO) {
                if (HIWORD(wParam) == CBN_SELCHANGE) {
                    currentTexture = (int)SendMessageA(hTextureCombo, CB_GETCURSEL, 0, 0);
                    InvalidateRect(hwnd, NULL, FALSE);
                } else if (HIWORD(wParam) == CBN_CLOSEUP) {
                    // When dropdown closes, return focus to main window
                    SetFocus(hwnd);
                }
            } else if (LOWORD(wParam) == IDC_FLOOR_TEXTURE_COMBO) {
                if (HIWORD(wParam) == CBN_SELCHANGE) {
                    currentFloorTexture = (int)SendMessageA(hFloorTextureCombo, CB_GETCURSEL, 0, 0);
                    InvalidateRect(hwnd, NULL, FALSE);
                } else if (HIWORD(wParam) == CBN_CLOSEUP) {
                    // When dropdown closes, return focus to main window
                    SetFocus(hwnd);
                }
            }
            break;
        }
        case WM_KEYDOWN: {
            if (wParam >= '0' && wParam <= '6') {
                currentTile = (int)(wParam - '0');
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (wParam == '7') {
                // Key 7 selects floor tiles (empty tiles)
                currentTile = 0; // Empty tile for floor
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (wParam == 'S') {
                char filename[MAX_PATH] = {0};
                if (showSaveDialog(hwnd, filename)) {
                    // Clean the filename by replacing spaces with underscores
                    char cleanFilename[MAX_PATH];
                    strcpy(cleanFilename, filename);
                    for (int i = 0; cleanFilename[i]; i++) {
                        if (cleanFilename[i] == ' ') {
                            cleanFilename[i] = '_';
                        }
                    }
                    
                    saveMap(cleanFilename);
                    strcpy(currentFile, cleanFilename);
                    // Extract just the filename for display
                    char *lastSlash = strrchr(cleanFilename, '\\');
                    if (lastSlash) {
                        strcpy(currentFileName, lastSlash + 1);
                    } else {
                        strcpy(currentFileName, cleanFilename);
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                SetFocus(hwnd); // Restore focus after dialog
            } else if (wParam == 'L') {
                char filename[MAX_PATH] = {0};
                if (showLoadDialog(hwnd, filename)) {
                    loadMap(filename);
                    strcpy(currentFile, filename);
                    // Extract just the filename for display
                    char *lastSlash = strrchr(filename, '\\');
                    if (lastSlash) {
                        strcpy(currentFileName, lastSlash + 1);
                    } else {
                        strcpy(currentFileName, filename);
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                SetFocus(hwnd); // Restore focus after dialog
            }
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
            
            int ox, oy, cell, size;
            calculateGridLayout(hwnd, &ox, &oy, &cell, &size);

            HBRUSH bg = CreateSolidBrush(RGB(10,10,10));
            RECT bgRect = { ox-1, oy-1, ox+size+1, oy+size+1 };
            FillRect(hdc, &bgRect, bg); DeleteObject(bg);

            for (int y = 0; y < MAP_H; ++y) {
                for (int x = 0; x < MAP_W; ++x) {
                    RECT r = { ox + x*cell, oy + y*cell, ox + (x+1)*cell-1, oy + (y+1)*cell-1 };
                    HBRUSH b = CreateSolidBrush(tileColor(mapData[y][x].wallType, mapData[y][x].textureId, mapData[y][x].floorTextureId));
                    FillRect(hdc, &r, b); DeleteObject(b);
                }
            }
            // grid lines
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(40,40,40)); HPEN old = SelectObject(hdc, pen);
            for (int i = 0; i <= MAP_W; ++i) {
                MoveToEx(hdc, ox + i*cell, oy, NULL); LineTo(hdc, ox + i*cell, oy + size);
                MoveToEx(hdc, ox, oy + i*cell, NULL); LineTo(hdc, ox + size, oy + i*cell);
            }
            SelectObject(hdc, old); DeleteObject(pen);

            // legend
            char legend[1024];
            const char* tileNames[] = {"Empty", "Wall Red", "Wall Green", "Wall Blue", "Wall Yellow", "Player", "Enemy"};
            wsprintfA(legend, "Tile: %d (%s) | Wall: %s | Floor: %s | File: %s | 0-6: Tile Type | 7: Floor | S: Save As  L: Load", 
                     currentTile, tileNames[currentTile], textureNames[currentTexture], floorTextureNames[currentFloorTexture], currentFileName);
            // Center the status text horizontally
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            SIZE textSize;
            GetTextExtentPoint32A(hdc, legend, (int)strlen(legend), &textSize);
            int textX = (clientRect.right - textSize.cx) / 2;
            TextOutA(hdc, textX, oy + size + 8, legend, (int)strlen(legend));

            EndPaint(hwnd, &ps);
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lp, int nCmd) {
    WNDCLASSA wc = {0}; wc.lpfnWndProc = WndProc; wc.hInstance = hInst; wc.lpszClassName = "RayWhenMapEdit"; wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); wc.hCursor=LoadCursor(NULL, IDC_ARROW);
    if (!RegisterClassA(&wc)) return 0;
    HWND hwnd = CreateWindowA(wc.lpszClassName, "RayWhen Map Editor", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 700, 700, NULL, NULL, hInst, NULL);
    if (!hwnd) return 0; 
    ShowWindow(hwnd, SW_SHOWDEFAULT); 
    UpdateWindow(hwnd);
    SetFocus(hwnd); // Ensure window has focus
    MSG msg; while (GetMessage(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessage(&msg);} return 0;
}


