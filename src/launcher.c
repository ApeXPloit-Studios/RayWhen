#include <windows.h>
#include <stdio.h>
#include <string.h>

#define IDC_WIDTH   1001
#define IDC_HEIGHT  1002
#define IDC_MOUSE   1003
#define IDC_PLAY    1004
#define IDC_PERF    1005
#define IDC_EDITMAP 1006
#define IDC_MAPFILE  1007
#define IDC_MAPCOMBO 1008
#define IDC_FPS     1009
#define IDC_RENDERER 1010

static const int kResolutions[][2] = {
    {640, 480}, {800, 600}, {1024, 768}, {1280, 720}, {1280, 800}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}
};

static const int kFPSOptions[] = {30, 60, 90, 120, 144};

static const char* kRendererOptions[] = {"Software", "DirectX 11"};

static HWND hWidthCombo, hHeightCombo, hMouseCheck, hPerfCheck, hPlayBtn, hEditBtn, hMapFileBtn, hMapCombo, hFPSCombo, hRendererCombo;
static char selectedMapFile[MAX_PATH] = "";
static char mapFiles[32][MAX_PATH];
static int numMaps = 0;

static void addItem(HWND combo, const char *text, int value) {
    SendMessageA(combo, CB_ADDSTRING, 0, (LPARAM)text);
    SendMessageA(combo, CB_SETITEMDATA, SendMessageA(combo, CB_GETCOUNT, 0, 0)-1, (LPARAM)value);
}

static void scanMapsDirectory() {
    numMaps = 0;
    WIN32_FIND_DATAA findData;
    
    // First scan for .rwm files (new format)
    HANDLE hFind = FindFirstFileA("maps\\*.rwm", &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                strcpy(mapFiles[numMaps], findData.cFileName);
                numMaps++;
                if (numMaps >= 32) break; // Prevent overflow
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
    
    // If no .rwm files found, scan for .txt files (backward compatibility)
    if (numMaps == 0) {
        hFind = FindFirstFileA("maps\\*.txt", &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    strcpy(mapFiles[numMaps], findData.cFileName);
                    numMaps++;
                    if (numMaps >= 32) break; // Prevent overflow
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
    }
    
    // Fallback if no maps found
    if (numMaps == 0) {
        strcpy(mapFiles[0], "map.rwm");
        numMaps = 1;
    }
}

static void populateMapCombo() {
    SendMessageA(hMapCombo, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < numMaps; i++) {
        SendMessageA(hMapCombo, CB_ADDSTRING, 0, (LPARAM)mapFiles[i]);
    }
    if (numMaps > 0) {
        SendMessageA(hMapCombo, CB_SETCURSEL, 0, 0);
        strcpy(selectedMapFile, mapFiles[0]);
    }
}

static int getSelectedValue(HWND combo) {
    int idx = (int)SendMessageA(combo, CB_GETCURSEL, 0, 0);
    if (idx < 0) return 0;
    return (int)SendMessageA(combo, CB_GETITEMDATA, idx, 0);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            CreateWindowA("STATIC", "RayWhen Launcher", WS_VISIBLE|WS_CHILD, 20, 15, 200, 20, hwnd, NULL, NULL, NULL);
            CreateWindowA("STATIC", "Width:", WS_VISIBLE|WS_CHILD, 20, 50, 60, 20, hwnd, NULL, NULL, NULL);
            hWidthCombo = CreateWindowA("COMBOBOX", "", WS_VISIBLE|WS_CHILD|CBS_DROPDOWNLIST, 80, 50, 120, 200, hwnd, (HMENU)IDC_WIDTH, NULL, NULL);
            CreateWindowA("STATIC", "Height:", WS_VISIBLE|WS_CHILD, 20, 80, 60, 20, hwnd, NULL, NULL, NULL);
            hHeightCombo = CreateWindowA("COMBOBOX", "", WS_VISIBLE|WS_CHILD|CBS_DROPDOWNLIST, 80, 80, 120, 200, hwnd, (HMENU)IDC_HEIGHT, NULL, NULL);
            CreateWindowA("STATIC", "FPS Target:", WS_VISIBLE|WS_CHILD, 20, 110, 60, 20, hwnd, NULL, NULL, NULL);
            hFPSCombo = CreateWindowA("COMBOBOX", "", WS_VISIBLE|WS_CHILD|CBS_DROPDOWNLIST, 80, 110, 120, 200, hwnd, (HMENU)IDC_FPS, NULL, NULL);
            CreateWindowA("STATIC", "Renderer:", WS_VISIBLE|WS_CHILD, 20, 140, 60, 20, hwnd, NULL, NULL, NULL);
            hRendererCombo = CreateWindowA("COMBOBOX", "", WS_VISIBLE|WS_CHILD|CBS_DROPDOWNLIST, 80, 140, 120, 200, hwnd, (HMENU)IDC_RENDERER, NULL, NULL);
            CreateWindowA("STATIC", "Map:", WS_VISIBLE|WS_CHILD, 20, 170, 60, 20, hwnd, NULL, NULL, NULL);
            hMapCombo = CreateWindowA("COMBOBOX", "", WS_VISIBLE|WS_CHILD|CBS_DROPDOWNLIST, 80, 170, 120, 200, hwnd, (HMENU)IDC_MAPCOMBO, NULL, NULL);
            hMouseCheck = CreateWindowA("BUTTON", "Enable Mouse Look", WS_VISIBLE|WS_CHILD|BS_AUTOCHECKBOX, 20, 205, 180, 22, hwnd, (HMENU)IDC_MOUSE, NULL, NULL);
            hPerfCheck  = CreateWindowA("BUTTON", "Performance Mode", WS_VISIBLE|WS_CHILD|BS_AUTOCHECKBOX, 20, 230, 180, 22, hwnd, (HMENU)IDC_PERF, NULL, NULL);
            hPlayBtn = CreateWindowA("BUTTON", "Play", WS_VISIBLE|WS_CHILD|BS_DEFPUSHBUTTON, 20, 265, 180, 28, hwnd, (HMENU)IDC_PLAY, NULL, NULL);
            hEditBtn = CreateWindowA("BUTTON", "Map Editor", WS_VISIBLE|WS_CHILD, 20, 300, 180, 26, hwnd, (HMENU)IDC_EDITMAP, NULL, NULL);
            hMapFileBtn = CreateWindowA("BUTTON", "Refresh Maps", WS_VISIBLE|WS_CHILD, 20, 335, 180, 26, hwnd, (HMENU)IDC_MAPFILE, NULL, NULL);

            char buf[32];
            for (int i = 0; i < (int)(sizeof(kResolutions)/sizeof(kResolutions[0])); ++i) {
                wsprintfA(buf, "%d", kResolutions[i][0]);
                addItem(hWidthCombo, buf, kResolutions[i][0]);
                wsprintfA(buf, "%d", kResolutions[i][1]);
                addItem(hHeightCombo, buf, kResolutions[i][1]);
            }
            SendMessageA(hWidthCombo, CB_SETCURSEL, 2, 0);  // 1024
            SendMessageA(hHeightCombo, CB_SETCURSEL, 2, 0); // 768
            
            // Add FPS options
            for (int i = 0; i < (int)(sizeof(kFPSOptions)/sizeof(kFPSOptions[0])); ++i) {
                wsprintfA(buf, "%d FPS", kFPSOptions[i]);
                addItem(hFPSCombo, buf, kFPSOptions[i]);
            }
            SendMessageA(hFPSCombo, CB_SETCURSEL, 1, 0); // 60 FPS default
            
            // Add renderer options
            for (int i = 0; i < (int)(sizeof(kRendererOptions)/sizeof(kRendererOptions[0])); ++i) {
                addItem(hRendererCombo, kRendererOptions[i], i);
            }
            SendMessageA(hRendererCombo, CB_SETCURSEL, 0, 0); // Software default
            
            // Scan and populate maps
            scanMapsDirectory();
            populateMapCombo();
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == IDC_PLAY) {
                int w = getSelectedValue(hWidthCombo);
                int h = getSelectedValue(hHeightCombo);
                int fps = getSelectedValue(hFPSCombo);
                int renderer = getSelectedValue(hRendererCombo);
                int m = (int)SendMessageA(hMouseCheck, BM_GETCHECK, 0, 0);
                int p = (int)SendMessageA(hPerfCheck,  BM_GETCHECK, 0, 0);

                char cmd[512];
                const char* rendererFlag = (renderer == 1) ? "-renderer dx11" : "-renderer software";
                // Always pass an explicit perf flag so the game doesn't auto-override
                if (strlen(selectedMapFile) > 0) {
                    wsprintfA(cmd, ".\\dist\\raywin.exe -map maps\\%s -w %d -h %d -fps %d %s %s %s", selectedMapFile, w, h, fps, rendererFlag, m ? "-mouselook" : "", p ? "-perf" : "--no-perf");
                } else {
                    wsprintfA(cmd, ".\\dist\\raywin.exe -w %d -h %d -fps %d %s %s %s", w, h, fps, rendererFlag, m ? "-mouselook" : "", p ? "-perf" : "--no-perf");
                }

                STARTUPINFOA si; PROCESS_INFORMATION pi;
                ZeroMemory(&si, sizeof(si));
                ZeroMemory(&pi, sizeof(pi));
                si.cb = sizeof(si);
                // Get current directory to pass as working directory
                char currentDir[MAX_PATH];
                GetCurrentDirectoryA(MAX_PATH, currentDir);
                BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, currentDir, &si, &pi);
                if (ok) {
                    CloseHandle(pi.hThread);
                    CloseHandle(pi.hProcess);
                    PostQuitMessage(0);
                } else {
                    MessageBoxA(hwnd, "Failed to launch game.", "Error", MB_ICONERROR);
                }
            } else if (LOWORD(wParam) == IDC_EDITMAP) {
                STARTUPINFOA si; PROCESS_INFORMATION pi; ZeroMemory(&si, sizeof(si)); ZeroMemory(&pi, sizeof(pi)); si.cb = sizeof(si);
                BOOL ok = CreateProcessA(NULL, "dist\\mapedit.exe", NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
                if (ok) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
            } else if (LOWORD(wParam) == IDC_MAPFILE) {
                // Refresh maps list
                scanMapsDirectory();
                populateMapCombo();
                
                char msg[128];
                sprintf(msg, "Refreshed maps list. Found %d map(s) in maps/ folder.", numMaps);
                MessageBoxA(hwnd, msg, "Maps Refreshed", MB_OK);
            } else if (LOWORD(wParam) == IDC_MAPCOMBO) {
                if (HIWORD(wParam) == CBN_SELCHANGE) {
                    int idx = (int)SendMessageA(hMapCombo, CB_GETCURSEL, 0, 0);
                    if (idx >= 0 && idx < numMaps) {
                        strcpy(selectedMapFile, mapFiles[idx]);
                    }
                }
            }
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "RayWhenLauncher";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    if (!RegisterClassA(&wc)) return 0;
    HWND hwnd = CreateWindowA(wc.lpszClassName, "RayWhen Launcher", WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 240, 410, NULL, NULL, hInstance, NULL);
    if (!hwnd) return 0;
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}


