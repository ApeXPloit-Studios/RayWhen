#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define IDC_WIDTH   1001
#define IDC_HEIGHT  1002
#define IDC_MOUSE   1003
#define IDC_PLAY    1004
#define IDC_PERF    1005
#define IDC_EDITMAP 1006
#define IDC_MAPFILE  1007
#define IDC_MAPCOMBO 1008
#define IDC_FPS     1009
#define IDC_FULLSCREEN 1010
#define IDC_DEBUG   1011

static const int kResolutions[][2] = {
    {640, 480}, {800, 600}, {1024, 768}, {1280, 720}, {1280, 800}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}
};

static const int kFPSOptions[] = {30, 60, 90, 120, 144};

static const char* kRendererOptions[] = {"Software"};

static HWND hWidthCombo, hHeightCombo, hMouseCheck, hPerfCheck, hPlayBtn, hEditBtn, hMapFileBtn, hMapCombo, hFPSCombo, hFullscreenCheck, hDebugCheck;
static char selectedMapFile[MAX_PATH] = "";
static char mapFiles[32][MAX_PATH];
static int numMaps = 0;

// Launcher settings structure
typedef struct {
    int width;
    int height;
    int fps;
    int mouseLook;
    int performance;
    int fullscreen;
    int debug;
    char mapFile[MAX_PATH];
} LauncherSettings;

static LauncherSettings settings = {
    .width = 1024,
    .height = 768,
    .fps = 60,
    .mouseLook = 0,
    .performance = 0,
    .fullscreen = 0,
    .debug = 0,
    .mapFile = ""
};

static void addItem(HWND combo, const char *text, int value) {
    SendMessageA(combo, CB_ADDSTRING, 0, (LPARAM)text);
    SendMessageA(combo, CB_SETITEMDATA, SendMessageA(combo, CB_GETCOUNT, 0, 0)-1, (LPARAM)value);
}

// Simple JSON-like settings file functions
static void saveSettings() {
    // Get AppData\Roaming path using environment variable
    char appDataPath[MAX_PATH];
    char settingsPath[MAX_PATH];
    
    if (GetEnvironmentVariableA("APPDATA", appDataPath, MAX_PATH) > 0) {
        // Create RayWhen directory if it doesn't exist
        wsprintfA(settingsPath, "%s\\RayWhen", appDataPath);
        CreateDirectoryA(settingsPath, NULL);
        
        // Full path to settings file
        wsprintfA(settingsPath, "%s\\RayWhen\\launcher_settings.json", appDataPath);
    } else {
        // Fallback to current directory
        strcpy(settingsPath, "launcher_settings.json");
    }
    
    FILE *file = fopen(settingsPath, "w");
    if (!file) return;
    
    fprintf(file, "{\n");
    fprintf(file, "  \"width\": %d,\n", settings.width);
    fprintf(file, "  \"height\": %d,\n", settings.height);
    fprintf(file, "  \"fps\": %d,\n", settings.fps);
    fprintf(file, "  \"mouseLook\": %d,\n", settings.mouseLook);
    fprintf(file, "  \"performance\": %d,\n", settings.performance);
    fprintf(file, "  \"fullscreen\": %d,\n", settings.fullscreen);
    fprintf(file, "  \"debug\": %d,\n", settings.debug);
    fprintf(file, "  \"mapFile\": \"%s\"\n", settings.mapFile);
    fprintf(file, "}\n");
    
    fclose(file);
}

static void loadSettings() {
    // Get AppData\Roaming path using environment variable
    char appDataPath[MAX_PATH];
    char settingsPath[MAX_PATH];
    
    if (GetEnvironmentVariableA("APPDATA", appDataPath, MAX_PATH) > 0) {
        // Full path to settings file
        wsprintfA(settingsPath, "%s\\RayWhen\\launcher_settings.json", appDataPath);
    } else {
        // Fallback to current directory
        strcpy(settingsPath, "launcher_settings.json");
    }
    
    FILE *file = fopen(settingsPath, "r");
    if (!file) return;
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Remove whitespace and newlines
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t' || *trimmed == '\n' || *trimmed == '\r') trimmed++;
        
        if (strstr(trimmed, "\"width\"")) {
            sscanf(trimmed, "\"width\": %d,", &settings.width);
        } else if (strstr(trimmed, "\"height\"")) {
            sscanf(trimmed, "\"height\": %d,", &settings.height);
        } else if (strstr(trimmed, "\"fps\"")) {
            sscanf(trimmed, "\"fps\": %d,", &settings.fps);
        } else if (strstr(trimmed, "\"mouseLook\"")) {
            sscanf(trimmed, "\"mouseLook\": %d,", &settings.mouseLook);
        } else if (strstr(trimmed, "\"performance\"")) {
            sscanf(trimmed, "\"performance\": %d,", &settings.performance);
        } else if (strstr(trimmed, "\"fullscreen\"")) {
            sscanf(trimmed, "\"fullscreen\": %d,", &settings.fullscreen);
        } else if (strstr(trimmed, "\"debug\"")) {
            sscanf(trimmed, "\"debug\": %d,", &settings.debug);
        } else if (strstr(trimmed, "\"mapFile\"")) {
            char *start = strchr(trimmed, '"');
            if (start) {
                start++; // Skip opening quote
                char *end = strrchr(start, '"');
                if (end) {
                    *end = '\0'; // Null terminate
                    strncpy(settings.mapFile, start, MAX_PATH - 1);
                    settings.mapFile[MAX_PATH - 1] = '\0';
                }
            }
        }
    }
    
    fclose(file);
}

static void scanMapsDirectory() {
    numMaps = 0;
    WIN32_FIND_DATAA findData;
    
    // Get executable directory and look for maps relative to it
    char exePath[MAX_PATH];
    char mapsPath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    
    // Find the last backslash and remove the executable name
    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash) {
        *lastSlash = '\0';
        // If we're in dist folder, maps are in maps subfolder
        if (strstr(exePath, "\\dist")) {
            wsprintfA(mapsPath, "%s\\maps\\*.rwm", exePath);
        } else {
            // If we're in project root, maps are in maps subfolder
            wsprintfA(mapsPath, "%s\\maps\\*.rwm", exePath);
        }
    } else {
        strcpy(mapsPath, "maps\\*.rwm");
    }
    
    // First scan for .rwm files (new format)
    HANDLE hFind = FindFirstFileA(mapsPath, &findData);
    
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
        // Update mapsPath for .txt files
        if (strstr(exePath, "\\dist")) {
            wsprintfA(mapsPath, "%s\\maps\\*.txt", exePath);
        } else {
            wsprintfA(mapsPath, "%s\\maps\\*.txt", exePath);
        }
        hFind = FindFirstFileA(mapsPath, &findData);
        
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
        // Try to select the saved map file
        int savedIndex = -1;
        if (strlen(settings.mapFile) > 0) {
            for (int i = 0; i < numMaps; i++) {
                if (strcmp(mapFiles[i], settings.mapFile) == 0) {
                    savedIndex = i;
                    break;
                }
            }
        }
        
        if (savedIndex >= 0) {
            SendMessageA(hMapCombo, CB_SETCURSEL, savedIndex, 0);
            strcpy(selectedMapFile, mapFiles[savedIndex]);
        } else {
            // Saved map not found, use first available map
            SendMessageA(hMapCombo, CB_SETCURSEL, 0, 0);
            strcpy(selectedMapFile, mapFiles[0]);
            // Update settings to reflect the actual selected map
            strcpy(settings.mapFile, selectedMapFile);
        }
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
            CreateWindowA("STATIC", "Map:", WS_VISIBLE|WS_CHILD, 20, 170, 60, 20, hwnd, NULL, NULL, NULL);
            hMapCombo = CreateWindowA("COMBOBOX", "", WS_VISIBLE|WS_CHILD|CBS_DROPDOWNLIST, 80, 170, 120, 200, hwnd, (HMENU)IDC_MAPCOMBO, NULL, NULL);
            hMouseCheck = CreateWindowA("BUTTON", "Enable Mouse Look", WS_VISIBLE|WS_CHILD|BS_AUTOCHECKBOX, 20, 205, 180, 22, hwnd, (HMENU)IDC_MOUSE, NULL, NULL);
            hPerfCheck  = CreateWindowA("BUTTON", "Performance Mode", WS_VISIBLE|WS_CHILD|BS_AUTOCHECKBOX, 20, 230, 180, 22, hwnd, (HMENU)IDC_PERF, NULL, NULL);
            hFullscreenCheck = CreateWindowA("BUTTON", "Fullscreen", WS_VISIBLE|WS_CHILD|BS_AUTOCHECKBOX, 20, 255, 180, 22, hwnd, (HMENU)IDC_FULLSCREEN, NULL, NULL);
            hDebugCheck = CreateWindowA("BUTTON", "Show Debug Info", WS_VISIBLE|WS_CHILD|BS_AUTOCHECKBOX, 20, 280, 180, 22, hwnd, (HMENU)IDC_DEBUG, NULL, NULL);
            hPlayBtn = CreateWindowA("BUTTON", "Play", WS_VISIBLE|WS_CHILD|BS_DEFPUSHBUTTON, 20, 315, 180, 28, hwnd, (HMENU)IDC_PLAY, NULL, NULL);
            hEditBtn = CreateWindowA("BUTTON", "Map Editor", WS_VISIBLE|WS_CHILD, 20, 350, 180, 26, hwnd, (HMENU)IDC_EDITMAP, NULL, NULL);
            hMapFileBtn = CreateWindowA("BUTTON", "Refresh Maps", WS_VISIBLE|WS_CHILD, 20, 385, 180, 26, hwnd, (HMENU)IDC_MAPFILE, NULL, NULL);

            char buf[32];
            for (int i = 0; i < (int)(sizeof(kResolutions)/sizeof(kResolutions[0])); ++i) {
                wsprintfA(buf, "%d", kResolutions[i][0]);
                addItem(hWidthCombo, buf, kResolutions[i][0]);
                wsprintfA(buf, "%d", kResolutions[i][1]);
                addItem(hHeightCombo, buf, kResolutions[i][1]);
            }
            // Load settings first
            loadSettings();
            
            // Set combo box selections based on saved settings
            for (int i = 0; i < (int)(sizeof(kResolutions)/sizeof(kResolutions[0])); ++i) {
                if (kResolutions[i][0] == settings.width) {
                    SendMessageA(hWidthCombo, CB_SETCURSEL, i, 0);
                    break;
                }
            }
            for (int i = 0; i < (int)(sizeof(kResolutions)/sizeof(kResolutions[0])); ++i) {
                if (kResolutions[i][1] == settings.height) {
                    SendMessageA(hHeightCombo, CB_SETCURSEL, i, 0);
                    break;
                }
            }
            
            // Add FPS options
            for (int i = 0; i < (int)(sizeof(kFPSOptions)/sizeof(kFPSOptions[0])); ++i) {
                wsprintfA(buf, "%d FPS", kFPSOptions[i]);
                addItem(hFPSCombo, buf, kFPSOptions[i]);
                if (kFPSOptions[i] == settings.fps) {
                    SendMessageA(hFPSCombo, CB_SETCURSEL, i, 0);
                }
            }
            
            // Set checkbox states
            SendMessageA(hMouseCheck, BM_SETCHECK, settings.mouseLook ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessageA(hPerfCheck, BM_SETCHECK, settings.performance ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessageA(hFullscreenCheck, BM_SETCHECK, settings.fullscreen ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessageA(hDebugCheck, BM_SETCHECK, settings.debug ? BST_CHECKED : BST_UNCHECKED, 0);
            
            
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
                int renderer = 0; // Always software
                int m = (int)SendMessageA(hMouseCheck, BM_GETCHECK, 0, 0);
                int p = (int)SendMessageA(hPerfCheck,  BM_GETCHECK, 0, 0);
                int f = (int)SendMessageA(hFullscreenCheck, BM_GETCHECK, 0, 0);
                int d = (int)SendMessageA(hDebugCheck, BM_GETCHECK, 0, 0);
                
                // Save current settings before launching
                settings.width = w;
                settings.height = h;
                settings.fps = fps;
                settings.mouseLook = m;
                settings.performance = p;
                settings.fullscreen = f;
                settings.debug = d;
                strcpy(settings.mapFile, selectedMapFile);
                saveSettings();

                char cmd[512];
                const char* rendererFlag = ""; // No renderer flag needed
                
                // Get executable directory to determine correct paths
                char exePath[MAX_PATH];
                char gameExePath[MAX_PATH];
                char mapPath[MAX_PATH];
                GetModuleFileNameA(NULL, exePath, MAX_PATH);
                
                // Find the last backslash and remove the executable name
                char* lastSlash = strrchr(exePath, '\\');
                if (lastSlash) {
                    *lastSlash = '\0';
                    if (strstr(exePath, "\\dist")) {
                        // We're in dist folder, game is in same folder
                        wsprintfA(gameExePath, "%s\\raywin.exe", exePath);
                        wsprintfA(mapPath, "maps\\%s", selectedMapFile);
                    } else {
                        // We're in project root, game is in dist folder
                        wsprintfA(gameExePath, "%s\\dist\\raywin.exe", exePath);
                        wsprintfA(mapPath, "maps\\%s", selectedMapFile);
                    }
                } else {
                    strcpy(gameExePath, ".\\dist\\raywin.exe");
                    wsprintfA(mapPath, "maps\\%s", selectedMapFile);
                }
                
                // Always pass an explicit perf flag so the game doesn't auto-override
                if (strlen(selectedMapFile) > 0) {
                    wsprintfA(cmd, "\"%s\" -map %s -w %d -h %d -fps %d %s %s %s %s", gameExePath, mapPath, w, h, fps, m ? "-mouselook" : "", p ? "-perf" : "--no-perf", f ? "-fullscreen" : "", d ? "-debug" : "");
                } else {
                    wsprintfA(cmd, "\"%s\" -w %d -h %d -fps %d %s %s %s %s", gameExePath, w, h, fps, m ? "-mouselook" : "", p ? "-perf" : "--no-perf", f ? "-fullscreen" : "", d ? "-debug" : "");
                }

                STARTUPINFOA si; PROCESS_INFORMATION pi;
                ZeroMemory(&si, sizeof(si));
                ZeroMemory(&pi, sizeof(pi));
                si.cb = sizeof(si);
                // Use executable directory as working directory
                char workingDir[MAX_PATH];
                strcpy(workingDir, exePath);
                
                BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, workingDir, &si, &pi);
                if (ok) {
                    CloseHandle(pi.hThread);
                    CloseHandle(pi.hProcess);
                    PostQuitMessage(0);
                } else {
                    MessageBoxA(hwnd, "Failed to launch game.", "Error", MB_ICONERROR);
                }
            } else if (LOWORD(wParam) == IDC_EDITMAP) {
                STARTUPINFOA si; PROCESS_INFORMATION pi; ZeroMemory(&si, sizeof(si)); ZeroMemory(&pi, sizeof(pi)); si.cb = sizeof(si);
                
                // Get executable directory and determine mapedit path
                char exePath2[MAX_PATH];
                char mapeditPath[MAX_PATH];
                char workingDir[MAX_PATH];
                GetModuleFileNameA(NULL, exePath2, MAX_PATH);
                
                // Find the last backslash and remove the executable name
                char* lastSlash2 = strrchr(exePath2, '\\');
                if (lastSlash2) {
                    *lastSlash2 = '\0';
                    strcpy(workingDir, exePath2);
                    
                    if (strstr(exePath2, "\\dist")) {
                        // We're in dist folder, mapedit is in same folder
                        wsprintfA(mapeditPath, "%s\\mapedit.exe", exePath2);
                    } else {
                        // We're in project root, mapedit is in dist folder
                        wsprintfA(mapeditPath, "%s\\dist\\mapedit.exe", exePath2);
                    }
                } else {
                    strcpy(mapeditPath, "dist\\mapedit.exe");
                    GetCurrentDirectoryA(MAX_PATH, workingDir);
                }
                
                BOOL ok = CreateProcessA(NULL, mapeditPath, NULL, NULL, FALSE, 0, NULL, workingDir, &si, &pi);
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
                        // Save map selection immediately
                        strcpy(settings.mapFile, selectedMapFile);
                        saveSettings();
                    }
                }
            }
            break;
        }
        case WM_DESTROY: {
            // Save current settings before exiting
            int w = getSelectedValue(hWidthCombo);
            int h = getSelectedValue(hHeightCombo);
            int fps = getSelectedValue(hFPSCombo);
            int m = (int)SendMessageA(hMouseCheck, BM_GETCHECK, 0, 0);
            int p = (int)SendMessageA(hPerfCheck, BM_GETCHECK, 0, 0);
            int f = (int)SendMessageA(hFullscreenCheck, BM_GETCHECK, 0, 0);
            int d = (int)SendMessageA(hDebugCheck, BM_GETCHECK, 0, 0);
            
            settings.width = w;
            settings.height = h;
            settings.fps = fps;
            settings.mouseLook = m;
            settings.performance = p;
            settings.fullscreen = f;
            settings.debug = d;
            strcpy(settings.mapFile, selectedMapFile);
            saveSettings();
            
            PostQuitMessage(0);
            break;
        }
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
    HWND hwnd = CreateWindowA(wc.lpszClassName, "RayWhen Launcher", WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 240, 450, NULL, NULL, hInstance, NULL);
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


