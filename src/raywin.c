#include "raywhen.h"
#include "texture.h"
#include "map.h"
#include "player.h"
#include "enemy.h"
#include "renderer.h"

// Global screen dimensions (will be updated on resize)
int SCREEN_WIDTH = DEFAULT_SCREEN_WIDTH;
int SCREEN_HEIGHT = DEFAULT_SCREEN_HEIGHT;

// FPS target (default 60 FPS)
int TARGET_FPS_VALUE = 60;

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

void ensureBackBuffer(HWND hwnd) {
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

// Command-line parsing for launcher options
void parseLaunchArgs(void) {
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
        if (strcmp(tok, "-fps") == 0 || strcmp(tok, "--fps") == 0) {
            char *next = strtok(NULL, " \t\r\n");
            if (next) {
                int fps = atoi(next);
                if (fps >= 30 && fps <= 144) TARGET_FPS_VALUE = fps;
            }
            tok = strtok(NULL, " \t\r\n");
            continue;
        }
        tok = strtok(NULL, " \t\r\n");
    }
    free(buf);
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static int keys[256] = {0};
    static double lastPlayerX = 8.5, lastPlayerY = 8.5, lastPlayerAngle = 0.0;
    static int flashFrames = 0;
    
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
            
            // Update player movement
            updatePlayerMovement(keys);
            
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
                    handleMouseLook(hwnd, dx, dy);
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
            // Cleanup software renderer resources
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
    
    char windowTitle[256];
    if (simpleShadingMode) {
        wsprintfA(windowTitle, "Advanced Raycasting Engine [PERF] [Software] [%d FPS] - WASD/Arrows to move, Shift to run", TARGET_FPS_VALUE);
    } else {
        wsprintfA(windowTitle, "Advanced Raycasting Engine [Software] [%d FPS] - WASD/Arrows to move, Shift to run", TARGET_FPS_VALUE);
    }
    
    HWND hwnd = CreateWindowEx(
        0,
        g_szClassName,
        windowTitle,
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

    // Performance mode is only set via command line arguments, no auto-scaling

    // Set up timer for smooth input handling (dynamic FPS)
    int timerInterval = 1000 / TARGET_FPS_VALUE;
    SetTimer(hwnd, 1, timerInterval, NULL);
    
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
