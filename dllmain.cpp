#include <windows.h>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <string>
#include <filesystem>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "rawdata.h"
#include <d3d11.h>
#include "spoofer.h"
#pragma comment(lib, "d3d11.lib")

using namespace std;

struct Config {
    string name;
    int minHSV[3] = { 0, 0, 0 };
    int maxHSV[3] = { 360, 255, 255 };
    int minRGB[3] = { 0, 0, 0 };
    int maxRGB[3] = { 255, 255, 255 };
    int aimOffset = 1;
};

vector<Config> configs = {
    {("Valorant PURPLE (best sort)"), {288, 106, 172}, {300, 255, 255}, {255, 255, 255}, {255, 255, 255}, 3},
    {("Valorant PURPLE (less sort)"), {288, 106, 172}, {302, 255, 255}, {255, 255, 255}, {255, 255, 255}, 3},
    {("Overwatch2 MAGENTA"),          {280, 128, 153}, {330, 255, 255}, {0,   0,   0  }, {255, 255, 255}, 25},
    {("Generic RED"),                 {0,   100, 100}, {10,  255, 255}, {150, 0,   0  }, {255, 100, 100}, 0},
    {("Generic YELLOW"),              {20,  100, 100}, {40,  255, 255}, {200, 200, 0  }, {255, 255, 100}, 0},
// {("DeltaForce Infrared"),         {0,   0,   0  }, {360, 255, 255}, {140, 170, 192}, {162, 213, 222}, -10},
// {("DeltaForce CampMode"),         {0,   0,   0  }, {360, 255, 255}, {35,  30,  170}, {49,  44,  245}, 50},
// delta force mode is from the code that i reverse engineered from a bot, but its not good enough, so i commented.
};

int currentConfigIndex = 0;
bool running = true;
std::atomic<bool> driverConnected = false;
bool botEnabled = true;
float fps = 0.0f;

float sensitivity = 3.43f;
float smoothing = 0.08f;
int fovWidth = 15;
int fovHeight = 15;

int aimKey = 0x06;
int flickKey = 0x05;
int triggerKey = 0x12;
int menuKey = VK_INSERT;
bool lastMenu = false;

bool randomJitter = true;
bool PredictionAim = false;
bool RapidFire = false;

float delay_Flick = 150;
float distance_Flick = 0;
float sentivity_Flick = 0.38;
int flick_mode = 0;

thread botThread;
BYTE* screenData = nullptr;
int screen_width, screen_height;
int aim_x = 0, aim_y = 0;
bool run_threads = true;

auto flick_start = std::chrono::high_resolution_clock::now();
auto flick_end = std::chrono::high_resolution_clock::now();
HANDLE hMainThread = NULL;

// ArdMouse* mouse = nullptr;

chrono::steady_clock::time_point lastTime;
int frameCount = 0;

void UpdateFPS();
void RunBot();
bool IsPressed(int key);
inline void RGBtoHSV(int r, int g, int b, double& h, double& s, double& v);
inline bool IsTargetColor(int red, int green, int blue);

void EraseHeaders(HINSTANCE hModule)
{
    SPOOF_FUNC
    PIMAGE_DOS_HEADER pDoH;
    PIMAGE_NT_HEADERS pNtH;
    DWORD i, ersize, protect;
    if (!hModule) return;
    DWORD dwOldProtect;
    VirtualProtect((LPVOID)hModule, 1024, PAGE_READWRITE, &dwOldProtect);
    pDoH = (PIMAGE_DOS_HEADER)(hModule);
    pDoH->e_magic = 0;
    pNtH = (PIMAGE_NT_HEADERS)((LONG)hModule + ((PIMAGE_DOS_HEADER)hModule)->e_lfanew);
    // pNtH->Signature = 0;
    VirtualProtect((LPVOID)hModule, 1024, dwOldProtect, &dwOldProtect);
    ersize = sizeof(IMAGE_DOS_HEADER);
    if (VirtualProtect(pDoH, ersize, PAGE_READWRITE, &protect))
    {
        for (i = 0; i < ersize; i++)
            *(BYTE*)((BYTE*)pDoH + i) = 0;
    }
    ersize = sizeof(IMAGE_NT_HEADERS);
    if (pNtH && VirtualProtect(pNtH, ersize, PAGE_READWRITE, &protect))
    {
        for (i = 0; i < ersize; i++)
            *(BYTE*)((BYTE*)pNtH + i) = 0;
    }
    return;
}

map<int, string> keyNames = {
    {0x01, "LMouse"}, {0x02, "RMouse"}, {0x04, "MidMouse"},
    {0x05, "XButton1"}, {0x06, "XButton2"},
    {VK_SHIFT, "Shift"}, {VK_CONTROL, "Ctrl"}, {VK_MENU, "Alt"},
    {VK_SPACE, "Space"}, {VK_CAPITAL, "Caps Lock"}, {VK_HOME, "HOME"},
    {VK_F1, "F1"}, {VK_F2, "F2"}, {VK_F3, "F3"}, {VK_F4, "F4"},
    {VK_F5, "F5"}, {VK_F6, "F6"}, {VK_F7, "F7"}, {VK_F8, "F8"},
    {VK_F9, "F9"}, {VK_F10, "F10"}, {VK_F11, "F11"}, {VK_F12, "F12"},
    {VK_INSERT, "INSERT" }
};

string GetKeyName(int key) {
    SPOOF_FUNC
    if (keyNames.find(key) != keyNames.end()) {
        return keyNames[key];
    }
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) {
        return string(1, (char)key);
    }
    return "" + to_string(key);
}

inline void RGBtoHSV(int r, int g, int b, double& h, double& s, double& v) {
    SPOOF_FUNC
    double rd = r / 255.0;
    double gd = g / 255.0;
    double bd = b / 255.0;
    double cmax = max(rd, max(gd, bd));
    double cmin = min(rd, min(gd, bd));
    double delta = cmax - cmin;

    if (delta == 0) {
        h = 0;
    }
    else if (cmax == rd) {
        h = 60 * fmod(((gd - bd) / delta), 6.0);
    }
    else if (cmax == gd) {
        h = 60 * (((bd - rd) / delta) + 2);
    }
    else {
        h = 60 * (((rd - gd) / delta) + 4);
    }

    if (h < 0) h += 360;
    if (h >= 360) h -= 360;
    s = (cmax == 0) ? 0 : (delta / cmax) * 255.0;
    v = cmax * 255.0;
}

inline bool IsTargetColor(int red, int green, int blue) {
    SPOOF_FUNC
        Config& cfg = configs[currentConfigIndex];

    bool rgbCondition = false;

    if (currentConfigIndex == 0 || currentConfigIndex == 1) // Valorant ONLY
    {
        // New improved RGB condition
        if (green >= 190)
            return false;

        if (green >= 140) {
            rgbCondition =
                abs(red - blue) <= 8 &&
                red - green >= 50 &&
                blue - green >= 50 &&
                red >= 105 &&
                blue >= 105;
        }
        else {
            rgbCondition =
                abs(red - blue) <= 13 &&
                red - green >= 60 &&
                blue - green >= 60 &&
                red >= 110 &&
                blue >= 100;
        }
    }
    else
    {
        // Keep original config-based RGB
        rgbCondition =
            (red >= cfg.minRGB[0] && red <= cfg.maxRGB[0] &&
                green >= cfg.minRGB[1] && green <= cfg.maxRGB[1] &&
                blue >= cfg.minRGB[2] && blue <= cfg.maxRGB[2]);
    }

    double h, s, v;
    RGBtoHSV(red, green, blue, h, s, v);

    int hue_int = static_cast<int>(h);
    int saturation_int = static_cast<int>(s);
    int value_int = static_cast<int>(v);

    bool hsvCondition =
        (hue_int >= cfg.minHSV[0] && hue_int <= cfg.maxHSV[0] &&
            saturation_int >= cfg.minHSV[1] && saturation_int <= cfg.maxHSV[1] &&
            value_int >= cfg.minHSV[2] && value_int <= cfg.maxHSV[2]);

    return rgbCondition && hsvCondition;
}


void RunBot() {
    SPOOF_FUNC
    screen_width = GetSystemMetrics(SM_CXSCREEN);
    screen_height = GetSystemMetrics(SM_CYSCREEN);

    HDC hScreen = GetDC(NULL);
    HBITMAP hBitmap = nullptr;
    BYTE* screenData = nullptr;
    HDC hDC = CreateCompatibleDC(hScreen);
    int lastW = 0, lastH = 0;
    BITMAPINFOHEADER bmi = { 0 };
    bmi.biSize = sizeof(BITMAPINFOHEADER);
    bmi.biPlanes = 1;
    bmi.biBitCount = 32;
    bmi.biCompression = BI_RGB;
    bmi.biSizeImage = 0;

    while (run_threads) {
        if (!botEnabled) {
            Sleep(100);
            continue;
        }

        Sleep(4);

        int w = (int)(fovWidth * (screen_width / 1920.0));
        int h = (int)(fovHeight * (screen_height / 1080.0));
        if (w != lastW || h != lastH) {
            if (screenData) {
                free(screenData);
                screenData = nullptr;
            }
            if (hBitmap) {
                DeleteObject(hBitmap);
                hBitmap = nullptr;
            }
            hBitmap = CreateCompatibleBitmap(hScreen, w, h);
            screenData = (BYTE*)malloc(w * h * 4);
            bmi.biWidth = w;
            bmi.biHeight = -h;
            lastW = w;
            lastH = h;
        }
        Config& cfg = configs[currentConfigIndex];
        HGDIOBJ old_obj = SelectObject(hDC, hBitmap);
        BitBlt(hDC, 0, 0, w, h, hScreen, screen_width / 2 - (w / 2), screen_height / 2 - (h / 2), SRCCOPY);

        SelectObject(hDC, old_obj);

        GetDIBits(hDC, hBitmap, 0, h, screenData, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);

        bool found = false;
        for (int j = 0; j < h; ++j) {
            for (int i = 0; i < w * 4; i += 4) {
                #define red screenData[i + (j*w*4) + 2]
                #define green screenData[i + (j*w*4) + 1]
                #define blue screenData[i + (j*w*4) + 0]

                if (IsTargetColor(red, green, blue)) {
                    aim_x = (i / 4) - (w / 2);
                    aim_y = j - (h / 2) + cfg.aimOffset;
                    found = true;
                    break;
                }
            }
            if (found) break;
        }

        if (!found) {
            aim_x = 0;
            aim_y = 0;
        }
    }

    free(screenData);
    DeleteDC(hDC);
    DeleteObject(hBitmap);
    ReleaseDC(NULL, hScreen);
}

bool IsPressed(int key) {
    SPOOF_FUNC
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

void UpdateFPS() {
    SPOOF_FUNC
    frameCount++;
    auto currentTime = chrono::steady_clock::now();
    auto elapsed = chrono::duration<float>(currentTime - lastTime).count();

    if (elapsed >= 1.0f) {
        fps = frameCount / elapsed;
        frameCount = 0;
        lastTime = currentTime;
    }
}

void RenderGUI() {
    SPOOF_FUNC
    Config& cfg = configs[currentConfigIndex];
    ImGui::Text("Last Updated: 2026.3.18 | Github: coderduc");
    // ImGui::SameLine();
    ImGui::Text("FPS: %.1f", fps);

    ImGui::Checkbox("Enable Colorbot", &botEnabled);
    ImGui::Separator();
    static const char* configNames[5];
    for (int i = 0; i < 5; i++) {
        configNames[i] = configs[i].name.c_str();
    }
    ImGui::Combo("##config", &currentConfigIndex, configNames, 5);
    ImGui::Separator();
    ImGui::SliderFloat("Sensitivity", &sensitivity, 0.1f, 5.0f, "%.2f");
    ImGui::SliderFloat("Smoothing", &smoothing, 0.01f, 1.0f, "%.2f");
    ImGui::SliderInt("FOV Width", &fovWidth, 15, 300);
    ImGui::SliderInt("FOV Height", &fovHeight, 15, 200);

	ImGui::SliderFloat("Flick Delay (ms)", &delay_Flick, 0, 500);
	ImGui::SliderFloat("Flick Distance", &distance_Flick, 0, 100);
	ImGui::SliderFloat("Flick Sensitivity", &sentivity_Flick, 0.1f, 2.0f, "%.2f");
	ImGui::SliderInt("Flick Mode", &flick_mode, 0, 1);

	ImGui::SliderFloat("Flick Delay (ms)", &delay_Flick, 0, 500);
	ImGui::SliderFloat("Flick Distance", &distance_Flick, 0, 100);
	ImGui::SliderFloat("Flick Sensitivity", &sentivity_Flick, 0.1f, 2.0f, "%.2f");

    ImGui::Separator();
    ImGui::Text("Keybinds:");
    static bool waitingForAimKey = false;
    static bool waitingForTriggerKey = false;
    static bool waitingForHideKey = false;
	static bool waitingForFlickKey = false;
    if (waitingForAimKey) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Press target key...");
        for (int i = 1; i < 256; i++) {
            if (IsPressed(i)) {
                aimKey = i;
                waitingForAimKey = false;
                break;
            }
        }
    }
    else {
        if (ImGui::Button(("Aimbot: " + GetKeyName(aimKey)).c_str())) {
            waitingForAimKey = true;
        }
    }
    ImGui::SameLine();
    if (waitingForTriggerKey) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Press target key...");
        for (int i = 1; i < 256; i++) {
            if (IsPressed(i)) {
                triggerKey = i;
                waitingForTriggerKey = false;
                break;
            }
        }
    }
    else {
        if (ImGui::Button(("Triggerbot: " + GetKeyName(triggerKey)).c_str())) {
            waitingForTriggerKey = true;
        }
    }
    ImGui::SameLine();
    if (waitingForFlickKey) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Press target key...");
        for (int i = 1; i < 256; i++) {
            if (IsPressed(i)) {
                flickKey = i;
                waitingForFlickKey = false;
                break;
            }
        }
    }
    else {
        if (ImGui::Button(("Flickbot: " + GetKeyName(flickKey)).c_str())) {
            waitingForFlickKey = true;
        }
    }
    ImGui::SameLine();
    if (waitingForHideKey) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Press target key...");
        for (int i = 1; i < 256; i++) {
            if (IsPressed(i)) {
                menuKey = i;
                waitingForHideKey = false;
                break;
            }
        }
    }
    else {
        if (ImGui::Button(("Menu: " + GetKeyName(menuKey)).c_str())) {
            waitingForHideKey = true;
        }
    }

    ImGui::Separator();
	ImGui::Checkbox("Random Jitter", &randomJitter);
    ImGui::SameLine();
	ImGui::Checkbox("Prediction Aim", &PredictionAim);
    ImGui::SameLine();
	ImGui::Checkbox("Rapid Trigger", &RapidFire);
    ImGui::SliderInt("Aim Offset", &cfg.aimOffset, 1, 8);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SPOOF_FUNC
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_CLOSE:
        running = false;
        DestroyWindow(hWnd);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

constexpr uint32_t CMD_MOUSE = 0x7BBA;

constexpr uint16_t MOUSE_LEFT_DOWN = 0x0001;
constexpr uint16_t MOUSE_LEFT_UP = 0x0002;
constexpr uint16_t MOUSE_RIGHT_DOWN = 0x0004;
constexpr uint16_t MOUSE_RIGHT_UP = 0x0008;
constexpr uint16_t MOUSE_MIDDLE_DOWN = 0x0010;
constexpr uint16_t MOUSE_MIDDLE_UP = 0x0020;

constexpr const wchar_t* COMM_VALUE_NAME = L"NvidiaPhysx";
constexpr const wchar_t* COMM_REG_PATH = L"SOFTWARE\\NvidiaPhysx";

// Hidden driver path (inside Fonts folder) – filled with Hangul filler (U+3164)
const std::wstring HIDDEN_DRIVER_PATH =
L"C:\\Windows\\Fonts\\ㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤ.ㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤ";

// Hidden service name – all filler characters
const std::wstring HIDDEN_SERVICE_NAME =
L"ㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤㅤ";

// ------------------------------------------------------------
// Structures for driver communication
// ------------------------------------------------------------
struct CommandHeader {
    uint32_t code;
    uint32_t _pad;
    void* request;
};

struct MouseRequest {
    int32_t dx;
    int32_t dy;
    uint16_t flags;
};

static_assert(sizeof(CommandHeader) == 16, "CommandHeader size mismatch");
static_assert(sizeof(MouseRequest) == 12, "MouseRequest size mismatch");

// ------------------------------------------------------------
// Driver loading / unloading
// ------------------------------------------------------------
bool drop_driver(const std::wstring& path) {
    SPOOF_FUNC
    HANDLE h_file = CreateFileW(
        path.c_str(),
        GENERIC_ALL,
        0,
        NULL,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (GetLastError() == ERROR_FILE_EXISTS)
        return true;

    if (h_file == INVALID_HANDLE_VALUE)
        return false;

    DWORD bytes_written = 0;
    BOOL b_status = WriteFile(h_file, driver_shellcode, sizeof(driver_shellcode), &bytes_written, nullptr);
    CloseHandle(h_file);
    return (b_status == TRUE);
}

void loadDriver() {
    SPOOF_FUNC
    // Drop the driver to the hidden path
    if (!drop_driver(HIDDEN_DRIVER_PATH))
        return;

    // Hide the file
    SetFileAttributesW(HIDDEN_DRIVER_PATH.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);

    // Open Service Control Manager
    SC_HANDLE hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hSCM) return;

    // Delete any existing service with the same hidden name
    SC_HANDLE hSvc = OpenServiceW(hSCM, HIDDEN_SERVICE_NAME.c_str(), SERVICE_STOP | DELETE);
    if (hSvc) {
        SERVICE_STATUS status;
        ControlService(hSvc, SERVICE_CONTROL_STOP, &status);
        DeleteService(hSvc);
        CloseServiceHandle(hSvc);
    }

    // Create the new service
    hSvc = CreateServiceW(
        hSCM,
        HIDDEN_SERVICE_NAME.c_str(),
        HIDDEN_SERVICE_NAME.c_str(),
        SERVICE_ALL_ACCESS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_IGNORE,
        HIDDEN_DRIVER_PATH.c_str(),
        NULL, NULL, NULL, NULL, NULL
    );

    if (!hSvc) {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS)
            hSvc = OpenServiceW(hSCM, HIDDEN_SERVICE_NAME.c_str(), SERVICE_ALL_ACCESS);
    }

    // Start the service
    if (hSvc) {
        StartServiceW(hSvc, 0, NULL);
        CloseServiceHandle(hSvc);
    }

    CloseServiceHandle(hSCM);
    Sleep(2000);
}

void unloadDriver() {
    SPOOF_FUNC
    // Stop and delete the hidden service
    SC_HANDLE hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (hSCM) {
        SC_HANDLE hSvc = OpenServiceW(hSCM, HIDDEN_SERVICE_NAME.c_str(), SERVICE_STOP | DELETE);
        if (hSvc) {
            SERVICE_STATUS status;
            ControlService(hSvc, SERVICE_CONTROL_STOP, &status);
            DeleteService(hSvc);
            CloseServiceHandle(hSvc);
        }
        CloseServiceHandle(hSCM);
    }

    // Remove the hidden file
    if (std::filesystem::exists(HIDDEN_DRIVER_PATH))
        std::filesystem::remove(HIDDEN_DRIVER_PATH);
}

// ------------------------------------------------------------
// Registry communication with the driver
// ------------------------------------------------------------
static HKEY  m_key = nullptr;
static bool  m_connected = false;

bool Connect() {
    SPOOF_FUNC
    if (m_connected) return true;

    DWORD disposition;
    LSTATUS status = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        COMM_REG_PATH,
        0, nullptr,
        REG_OPTION_VOLATILE,
        KEY_SET_VALUE | KEY_QUERY_VALUE,
        nullptr,
        &m_key,
        &disposition
    );

    if (status != ERROR_SUCCESS) return false;
    m_connected = true;
    driverConnected = true;
    return true;
}

void Disconnect() {
    SPOOF_FUNC
    if (!m_connected) return;
    RegDeleteValueW(m_key, COMM_VALUE_NAME);
    RegCloseKey(m_key);
    m_key = nullptr;
    m_connected = false;
    driverConnected = false;
}

bool SendCommand(CommandHeader* cmd) {
    SPOOF_FUNC
    if (!m_connected) return false;
    uint64_t ptr = reinterpret_cast<uint64_t>(cmd);
    LSTATUS status = RegSetValueExW(
        m_key,
        COMM_VALUE_NAME,
        0,
        REG_QWORD,
        reinterpret_cast<const BYTE*>(&ptr),
        sizeof(ptr)
    );
    return (status == ERROR_SUCCESS);
}

bool MouseMove(int32_t dx, int32_t dy, uint16_t flags) {
    SPOOF_FUNC
    MouseRequest req{ dx, dy, flags };
    CommandHeader cmd{ CMD_MOUSE, 0, &req };
    return SendCommand(&cmd);
}

int main(void) {
    SPOOF_FUNC
    // Load the kernel driver
    loadDriver();

    // Connect to the driver via registry
    if (!Connect()) {
        MessageBoxA(NULL, "Failed to connect to driver.", "Error", MB_ICONERROR);
        return 1;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2); // cnmsb windows
    // adapt the monitor dpi, fuck this prob
    int WINDOW_HEIGHT = 390;
	int WINDOW_WIDTH = 580;

    // do not use wide char func due to xorstr
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"Pixmenu_jhl337", NULL };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, (L"Pixmenu"), WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX
        , 100, 100, WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL, wc.hInstance, NULL);
    if (!hwnd) return 1;

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* deviceContext = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* mainRenderTargetView = nullptr;

    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, NULL, 0, D3D11_SDK_VERSION, &sd, &swapChain, &device, &featureLevel, &deviceContext) != S_OK)
        return 1;
    ID3D11Texture2D* pBackBuffer = nullptr;
    swapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        device->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
        pBackBuffer->Release();
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(device, deviceContext);

    // mouse = getMouse();
    botThread = thread(RunBot);
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    lastTime = chrono::steady_clock::now();

    CURSORINFO cursorInfo;
    auto t_start = std::chrono::high_resolution_clock::now();
    auto t_end = std::chrono::high_resolution_clock::now();
    auto left_start = std::chrono::high_resolution_clock::now();
    auto left_end = std::chrono::high_resolution_clock::now();
    int move_times = 0;
    bool visible = true;
    while (running && msg.message != WM_QUIT) {

        if (GetAsyncKeyState(VK_END) & 1)
        {
            running = false;
        }

        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }
        bool now = IsPressed(menuKey);
        if (now && !lastMenu) {
            if (visible) {
                ShowWindow(hwnd, SW_HIDE);
            }
            else {
                ShowWindow(hwnd, SW_SHOWDEFAULT);
            }
            visible = !visible;
        }
        lastMenu = now;
        if (visible)
        {
            // if hiding, do not render
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, WINDOW_HEIGHT));
            ImGui::Begin(("ColorBot"), &running,
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoTitleBar);
            RenderGUI();
            ImGui::End();
            ImGui::Render();
            const float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
            deviceContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
            deviceContext->ClearRenderTargetView(mainRenderTargetView, clearColor);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            swapChain->Present(1, 0);
            UpdateFPS();
        }

        

        t_end = std::chrono::high_resolution_clock::now();
        double elapsed_time_ms = chrono::duration<double, milli>(t_end - t_start).count();

        cursorInfo = { 0 };
        cursorInfo.cbSize = sizeof(cursorInfo);
        GetCursorInfo(&cursorInfo);
        if (cursorInfo.flags == 1) {
			Sleep(1);
			continue;
        }

        if (botEnabled) {
            bool isLeftKeyDown = IsPressed(VK_LBUTTON);
            if (!isLeftKeyDown) {
                left_end = std::chrono::high_resolution_clock::now();
            }
            if (isLeftKeyDown) {
                if (elapsed_time_ms > 7) {
                    t_start = std::chrono::high_resolution_clock::now();
                    left_start = std::chrono::high_resolution_clock::now();
                    if (aim_x != 0 || aim_y != 0) {
                        if (fabs(move_times) > 65 && PredictionAim) {
                            // do predict move
                            double e_x = move_times > 0 ? 1 : -1;
							MouseMove((int)e_x, 0, 0);
                        }
                    }
                }
            }
            if (elapsed_time_ms > 7) {
                t_start = std::chrono::high_resolution_clock::now();
                if (aim_x != 0 || aim_y != 0) {
                    if (IsPressed(triggerKey) && !isLeftKeyDown)
                    {
                        if (RapidFire)
                        {
							MouseMove(0, 0, MOUSE_LEFT_DOWN);
							MouseMove(0, 0, MOUSE_LEFT_UP);
                            Sleep(1);
                            MouseMove(0, 0, MOUSE_LEFT_DOWN);
                            MouseMove(0, 0, MOUSE_LEFT_UP);
                        }
                        else
                        {
                            MouseMove(0, 0, MOUSE_LEFT_DOWN);
                            Sleep(3);
                            MouseMove(0, 0, MOUSE_LEFT_UP);
                            Sleep(1);
                        }
                    }

                    //Aimbot
                    if (IsPressed(aimKey)) {
                        double recoil_ms = std::chrono::duration<double, std::milli>(left_start - left_end).count();
                        double extra = 38.0 * (screen_height / 1080.0) * (recoil_ms / 1000.0);
                        if (!isLeftKeyDown) extra = 0;
                        else if (extra > 38.0) extra = 38.0;

                        double v_x = double(aim_x) * sensitivity * smoothing;
                        double v_y = double(aim_y + extra) * sensitivity * smoothing;
                        if (fabs(v_x) < 1.0 && randomJitter) {
                            v_x = v_x > 0 ? 1.05 : -1.05;
                        }
                        else if (fabs(v_x) >= 1.0 && PredictionAim) {
                            if ((move_times > 0 && v_x < 0) || (move_times < 0 && v_x > 0)) {
                                move_times = 0;
                            }
                            move_times = v_x > 0 ? move_times + 1 : move_times - 1;
                            if (fabs(move_times) > 20) {
                                double extra_x = move_times > 0 ? 5 : -5;
                                v_x += extra_x;
                            }
                        }
                        if (fabs(v_y) < 1.0 && randomJitter) {
                            v_y = v_y > 0 ? 1.05 : -1.05;
                        }
						MouseMove((int)v_x, (int)v_y, 0);
                    }
                    else {
                        move_times = 0;
                    }
                }
            }
        }
    }

    run_threads = false;
    if (botThread.joinable()) {
        botThread.join();
    }

    if (screenData) {
        free(screenData);
    }

    SafeUnloadDriver();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (mainRenderTargetView) mainRenderTargetView->Release();
    if (swapChain) swapChain->Release();
    if (deviceContext) deviceContext->Release();
    if (device) device->Release();

    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hDllModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    SPOOF_FUNC
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hDllModule);
        hMainThread = CreateThread(0, 0, MainThread, 0, 0, 0);
		EraseHeaders(hDllModule);
    }
    if (ul_reason_for_call == DLL_PROCESS_DETACH)
    {
        running = false;

        if (hMainThread)
        {
            WaitForSingleObject(hMainThread, 3000);
            CloseHandle(hMainThread);
        }

        SafeUnloadDriver();
    }
    return TRUE;
}
