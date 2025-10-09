﻿#define NOMINMAX
#include <windows.h>
#include "resource.h"

#include <iostream>
#include <vector>
#include <processthreadsapi.h>
#include <tchar.h>
#include <thread>
#include <chrono>
#include <string>
#include <atomic>
#include <algorithm>  
#include <tlhelp32.h>
#include <d3d11.h>
#include <shlwapi.h>
#include <random>

#include "imgui-files/imgui.h"
#include "imgui-files/imgui_impl_dx11.h"
#include "imgui-files/imgui_impl_win32.h"
#include "imgui-files/json.hpp"

#include "Resource Files/miniz.h"
#include "Resource Files/keymapping.h"
#include "Resource Files/wine_compatibility_layer.h"
#include "Resource Files/save_system.h"

#include <comdef.h>
#include <shlobj.h>
#include <condition_variable>
#include <fstream>
#include <filesystem>
#include <locale>
#include <codecvt>
#include <format>
#include <unordered_map>
#include <math.h>
#include <memory.h>
#include <synchapi.h>
#include <dwmapi.h>
#include <variant>
#include <shellscalingapi.h>

// Generic libraries for I forgot

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "Shcore.lib")

using json = nlohmann::json;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// DirectX11 Variables
ID3D11Device *g_pd3dDevice = NULL;
ID3D11DeviceContext *g_pd3dDeviceContext = NULL;
IDXGISwapChain *g_pSwapChain = NULL;
ID3D11RenderTargetView *g_mainRenderTargetView = NULL;

// Forward Declarations
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// TO PUT IN A KEYBOARD KEY, GO TO https://www.millisecond.com/support/docs/current/html/language/scancodes.htm
// Convert the scancode into hexadecimal before putting it into the HoldKey or ReleaseKey functions
// Ex: E = 18 = 0x12 = HoldKey(0x12)

// If you want to create custom HOTKEYS for stuff that isn't an alphabet/function key, go to https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes and get your virtual key code or value

std::string text = "/e dance2"; // Custom chat text

std::atomic<bool> isdesyncloop(false); // Set the variable used for the threads
std::atomic<bool> isspeed(false);
std::atomic<bool> isHHJ(false);
std::atomic<bool> isspamloop(false);
std::atomic<bool> isitemloop(false);
std::atomic<bool> iswallwalkloop(false);
std::atomic<bool> isbhoploop(false);
std::atomic<bool> isafk(false);
std::atomic<bool> iswallhopthread(false);
std::atomic<bool> ispresskeythread(false);

std::atomic<unsigned int> RobloxFPS(120);

std::mutex renderMutex;
std::condition_variable renderCondVar;
bool renderFlag = false;
bool running = true;
HWND hwnd;

std::atomic<bool> g_isVk_BunnyhopHeldDown(false);
HHOOK g_keyboardHook = NULL;

const DWORD SCAN_CODE_FLAGS = KEYEVENTF_SCANCODE;
const DWORD RELEASE_FLAGS = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;

INPUT inputkey = {};
INPUT inputhold = {};
INPUT inputrelease = {};

// Translate all VK's into variables so it's less annoying to debug it
unsigned int vk_f5 = VK_F5;
unsigned int vk_f6 = VK_F6;
unsigned int vk_f8 = VK_F8;
unsigned int vk_mbutton = VK_MBUTTON;
unsigned int vk_xbutton1 = VK_XBUTTON1;
unsigned int vk_xbutton2 = VK_XBUTTON2;
unsigned int vk_spamkey = VK_LBUTTON;
unsigned int vk_clipkey = VK_F3;
unsigned int vk_wallkey = VK_F1;
unsigned int vk_laughkey = VK_F7;
// Use this for alphabet keys so it works across layouts
unsigned int vk_zkey = VkKeyScanEx('Z', GetKeyboardLayout(0)) & 0xFF;
unsigned int vk_dkey = VkKeyScanEx('D', GetKeyboardLayout(0)) & 0xFF;
unsigned int vk_xkey = VkKeyScanEx('X', GetKeyboardLayout(0)) & 0xFF;
unsigned int vk_wkey = VkKeyScanEx('W', GetKeyboardLayout(0)) & 0xFF;
unsigned int vk_bouncekey = VkKeyScanEx('C', GetKeyboardLayout(0)) & 0xFF;
unsigned int vk_leftbracket = MapVirtualKey(0x1A, MAPVK_VSC_TO_VK);
unsigned int vk_bunnyhopkey = MapVirtualKey(0x39, MAPVK_VSC_TO_VK);
unsigned int vk_shiftkey = VK_SHIFT;
unsigned int vk_enterkey = VK_RETURN;
unsigned int vk_chatkey = VkKeyScanEx('/', GetKeyboardLayout(0)) & 0xFF;
unsigned int vk_afkkey = VkKeyScanEx('\\', GetKeyboardLayout(0)) & 0xFF;

// ADD KEYBIND VARIABLES FOR EACH SECTION
static const std::unordered_map<int, unsigned int *> section_to_key = {
	{0, &vk_mbutton},    {1, &vk_f5},           {2, &vk_xbutton1}, {3, &vk_xkey},
	{4, &vk_f8},         {5, &vk_zkey},         {6, &vk_xbutton2}, {7, &vk_f6},
	{8, &vk_clipkey},    {9, &vk_laughkey},     {10, &vk_wallkey}, {11, &vk_leftbracket},
	{12, &vk_bouncekey}, {13, &vk_bunnyhopkey}};


const std::string G_SETTINGS_FILEPATH = "RMCSettings.json";
static std::string G_CURRENTLY_LOADED_PROFILE_NAME = "";

const std::string METADATA_KEY = "_metadata";
const std::string LAST_ACTIVE_PROFILE_KEY = "last_active_profile";

// Lookup Table - Backup to avoid displaying Hex if unneccessary
std::unordered_map<int, std::string> vkToString = {
    {VK_LBUTTON, "VK_LBUTTON"},
    {VK_RBUTTON, "VK_RBUTTON"},
    {VK_CANCEL, "VK_CANCEL"},
    {VK_MBUTTON, "VK_MBUTTON"},
    {VK_XBUTTON1, "VK_XBUTTON1"},
    {VK_XBUTTON2, "VK_XBUTTON2"},
    {VK_BACK, "VK_BACK"},
    {VK_TAB, "VK_TAB"},
    {VK_CLEAR, "VK_CLEAR"},
    {VK_RETURN, "VK_RETURN"},
    {VK_SHIFT, "VK_SHIFT"},
    {VK_CONTROL, "VK_CONTROL"},
    {VK_MENU, "VK_MENU"},
    {VK_PAUSE, "VK_PAUSE"},
    {VK_CAPITAL, "VK_CAPITAL"},
    {VK_ESCAPE, "VK_ESCAPE"},
    {VK_SPACE, "VK_SPACE"},
    {VK_PRIOR, "VK_PRIOR"},
    {VK_NEXT, "VK_NEXT"},
    {VK_END, "VK_END"},
    {VK_HOME, "VK_HOME"},
    {VK_LEFT, "VK_LEFT"},
    {VK_UP, "VK_UP"},
    {VK_RIGHT, "VK_RIGHT"},
    {VK_DOWN, "VK_DOWN"},
    {VK_SELECT, "VK_SELECT"},
    {VK_PRINT, "VK_PRINT"},
    {VK_EXECUTE, "VK_EXECUTE"},
    {VK_SNAPSHOT, "VK_SNAPSHOT"},
    {VK_INSERT, "VK_INSERT"},
    {VK_DELETE, "VK_DELETE"},
    {VK_HELP, "VK_HELP"},
    {VK_LWIN, "VK_LWIN"},
    {VK_RWIN, "VK_RWIN"},
    {VK_NUMPAD0, "VK_NUMPAD0"},
    {VK_NUMPAD1, "VK_NUMPAD1"},
    {VK_NUMPAD2, "VK_NUMPAD2"},
    {VK_NUMPAD3, "VK_NUMPAD3"},
    {VK_NUMPAD4, "VK_NUMPAD4"},
    {VK_NUMPAD5, "VK_NUMPAD5"},
    {VK_NUMPAD6, "VK_NUMPAD6"},
    {VK_NUMPAD7, "VK_NUMPAD7"},
    {VK_NUMPAD8, "VK_NUMPAD8"},
    {VK_NUMPAD9, "VK_NUMPAD9"},
    {VK_MULTIPLY, "VK_MULTIPLY"},
    {VK_ADD, "VK_ADD"},
    {VK_SEPARATOR, "VK_SEPARATOR"},
    {VK_SUBTRACT, "VK_SUBTRACT"},
    {VK_DECIMAL, "VK_DECIMAL"},
    {VK_DIVIDE, "VK_DIVIDE"},
    {VK_LSHIFT, "VK_LSHIFT"},
    {VK_RSHIFT, "VK_RSHIFT"},
    {VK_LCONTROL, "VK_LCONTROL"},
    {VK_RCONTROL, "VK_RCONTROL"},
    {VK_LMENU, "VK_LMENU"},
    {VK_RMENU, "VK_RMENU"},
    {VK_OEM_PLUS, "VK_OEM_PLUS"},
    {VK_OEM_COMMA, "VK_OEM_COMMA"},
    {VK_OEM_MINUS, "VK_OEM_MINUS"},
    {VK_OEM_PERIOD, "VK_OEM_PERIOD"},
    {VK_OEM_2, "VK_OEM_2"},
    {VK_OEM_3, "VK_OEM_3"},
    {VK_OEM_4, "VK_OEM_4"},
    {VK_OEM_5, "VK_OEM_5"},
    {VK_OEM_6, "VK_OEM_6"},
    {VK_OEM_7, "VK_OEM_7"},
    {VK_OEM_8, "VK_OEM_8"},
    {VK_OEM_102, "VK_OEM_102"},
	{0x0, "RESTART PC"},
	{256, "M_WHEEL_UP"},
	{257, "M_WHEEL_DOWN"}
};

// Window and UI settings
int screen_width = GetSystemMetrics(SM_CXSCREEN) / 1.5;
int screen_height = GetSystemMetrics(SM_CYSCREEN) / 1.5 + 10;
int raw_window_width;
int raw_window_height;
int selected_dropdown = 0;
std::string KeyButtonText = "Click to Bind Key";
std::string KeyButtonTextalt = "Click to Bind Key";

// Keybind and macro settings
char settingsBuffer[256] = "RobloxPlayerBeta.exe"; // Default value for the textbox
char KeyBuffer[256] = "None";
char KeyBufferalt[256] = "None";
char KeyBufferhuman[256] = "None";
char KeyBufferhumanalt[256] = "None";
char ItemDesyncSlot[256] = "5";
char ItemSpeedSlot[256] = "3";
char ItemClipSlot[256] = "7";
char ItemClipDelay[256] = "34";
char BunnyHopDelayChar[256] = "10";
char WallhopPixels[256] = "300";
char WallhopDelayChar[256] = "19";
char WallhopBonusDelayChar[256] = "0";
char WallhopDegrees[256] = "150";
char SpamDelay[256] = "20";
char RobloxSensValue[256] = "0.5";
char RobloxPixelValueChar[256] = "716";
char RobloxWallWalkValueChar[256] = "-94";
char RobloxWallWalkValueDelayChar[256] = "72720";
char CustomTextChar[8192] = "";
char RobloxFPSChar[256] = "60";
char AntiAFKTimeChar[256] = "15";
char PressKeyDelayChar[256] = "16";
char PressKeyBonusDelayChar[256] = "0";
char PasteDelayChar[256] = "1";
char HHJLengthChar[16] = "243";
char HHJFreezeDelayOverrideChar[16] = "0";
char HHJDelay1Char[16] = "9";
char HHJDelay2Char[16] = "17";
char HHJDelay3Char[16] = "16";


// Toggles and switches
bool macrotoggled = true;
bool shiftswitch = false;
bool unequiptoggle = false;
bool camfixtoggle = false;
bool wallhopswitch = false;
bool wallwalktoggleside = false;
bool wallhopcamfix = false;
bool chatoverride = true;
bool toggle_jump = true;
bool toggle_flick = true;
bool fasthhj = false;
bool hhjzoomin = false;
bool hhjzoominreverse = false;
bool laughzoomin = false;
bool laughzoominreverse = false;
bool lhjzoomin = false;
bool lhjzoominreverse = false;
bool wallesslhjswitch = false;
bool autotoggle = false;
bool isspeedswitch = false;
bool isfreezeswitch = false;
bool freezeoutsideroblox = true;
bool iswallwalkswitch = false;
bool isspamswitch = false;
bool isitemclipswitch = false;
bool antiafktoggle = true;
bool bouncesidetoggle = false;
bool bouncerealignsideways = true;
bool bounceautohold = true;
bool laughmoveswitch = false;
bool takeallprocessids = false;
bool ontoptoggle = false;
bool bunnyhoptoggled = false;
bool bunnyhopsmart = true;
bool presskeyinroblox = false;
bool unequipinroblox = false;
bool shortdescriptions = false;
bool useoldpaste = true;
bool doublepressafkkey = true;

// Section toggles and order
constexpr int section_amounts = 14;
bool section_toggles[14] = {true, true, true, true, true, false, true, true, true, false, false, false, false, false};
int section_order[14] = {0, 1, 2, 3, 4, 5, 6, 13, 7, 8, 9, 10, 11, 12};

// Numeric settings
int wallhop_dx = 300;
int wallhop_dy = -300;
int speed_strengthx = 959;
int wallwalk_strengthx = 94;
int speedoffsetx = 0;
int speed_strengthy = -959;
int wallwalk_strengthy = -94;
int speedoffsety = 0;
int speed_slot = 3;
int desync_slot = 5;
int clip_slot = 7;
int clip_delay = 30;
int BunnyHopDelay = 10;
int RobloxWallWalkValueDelay = 72720;
float spam_delay = 20.0f;
float maxfreezetime = 9.00f;
int maxfreezeoverride = 50;
int real_delay = 1000;
int RobloxPixelValue = 716;
int RobloxWallWalkValue = -94;
int WallhopDelay = 17;
int WallhopBonusDelay = 0;
int AntiAFKTime = 15;
int display_scale = 100;
int PressKeyDelay = 16;
int PressKeyBonusDelay = 0;
int WindowPosX = 0;
int WindowPosY = 0;
int PasteDelay = 1;
int HHJLength = 243;
int HHJFreezeDelayOverride = 0;
int HHJDelay1 = 9;
int HHJDelay2 = 17;
int HHJDelay3 = 16;

// Dropdown options
const char* optionsforoffset[] = {"/e dance2", "/e laugh", "/e cheer"};

// Window and UI state
RECT screen_rect;
int dragged_section = -1; // -1 means no section is being dragged
static int selected_section = -1;

// Process and timing state
bool processFound = false; // Initialize as no process found
bool done = false;
bool bindingMode = false;
bool bindingModealt = false;
bool notbinding = true;
bool wallhopupdate = false;
bool UserOutdated = false;
static bool wasMButtonPressed = false;

// Timing and chrono
auto rebindtime = std::chrono::steady_clock::now();
auto suspendStartTime = std::chrono::steady_clock::time_point();

// Previous values (used for comparisons)
static float PreviousSensValue = -1.0f;
static float PreviousWallWalkSide = 0;
static float PreviousWallWalkValue = 0.5f;
static float windowOpacityPercent = 100.0f;

static INPUT createInput()
{
	INPUT inputkey = {};
	inputkey.type = INPUT_KEYBOARD;
	return inputkey;
}

// This is ran in a separate thread to avoid interfering with other functions

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        // Reset AFK on any key activity
        isafk.store(false, std::memory_order_relaxed);
        
        const KBDLLHOOKSTRUCT* pkbhs = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        if (pkbhs->vkCode == vk_bunnyhopkey) {
            if ((pkbhs->flags & LLKHF_INJECTED) == 0) {
                // Use relaxed for the bunnyhop flag
                g_isVk_BunnyhopHeldDown.store((wParam & 1) == 0, std::memory_order_relaxed);
            }
        }
    }
    return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
}

static void setBunnyHopState(bool state) {
	g_isVk_BunnyhopHeldDown.store(state, std::memory_order_relaxed); // For linux compatibility
}

static void KeyboardHookThread() 
{
	HINSTANCE hMod = GetModuleHandle(NULL);
	g_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hMod, 0);
	// Message handler for this thread for the keyboard hook only
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	
	UnhookWindowsHookEx(g_keyboardHook);
}

// Special class to allow mouse scroll events to be found and used
class MouseWheelHandler {
private:
    std::atomic<bool> wheelUp{false};
    std::atomic<bool> wheelDown{false};

    void SetWheelUp() {
        wheelUp.store(true);
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            wheelUp.store(false);
        }).detach();
    }

    void SetWheelDown() {
        wheelDown.store(true);
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            wheelDown.store(false);
        }).detach();
    }

public:
    // Call this once when GUI initializes
    bool Initialize(HWND windowhandle) {
        RAWINPUTDEVICE rid;
        rid.usUsagePage = 0x01; // Generic desktop
        rid.usUsage = 0x02;     // Mouse
        rid.dwFlags = RIDEV_INPUTSINK;
        rid.hwndTarget = windowhandle; // The GUI window
        
        return RegisterRawInputDevices(&rid, 1, sizeof(rid));
    }

    // Call this from WndProc when you get WM_INPUT
    void ProcessRawInput(LPARAM lParam) {
        UINT dwSize = 0;
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
        
        if (dwSize > 0) {
            LPBYTE lpb = new BYTE[dwSize];
            if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) == dwSize) {
                RAWINPUT* raw = (RAWINPUT*)lpb;
                
                if (raw->header.dwType == RIM_TYPEMOUSE) {
                    if (raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL) {
                        short wheelDelta = (short)raw->data.mouse.usButtonData;
                        if (wheelDelta > 0) {
                            SetWheelUp();
                        } else if (wheelDelta < 0) {
                            SetWheelDown();
                        }
                    }
                }
            }
            delete[] lpb;
        }
    }

    bool IsWheelUp() const { return wheelUp.load(); }
    bool IsWheelDown() const { return wheelDown.load(); }
};

MouseWheelHandler g_mouseWheel;

// Tell the other file containg IsKeyPressed that these functionse exist 
bool IsWheelUp() { return g_mouseWheel.IsWheelUp(); }
bool IsWheelDown() { return g_mouseWheel.IsWheelDown(); }

static void ItemDesyncLoop()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
	while (true) { // Efficient variable checking method
		while (!isdesyncloop) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		if (macrotoggled && notbinding && section_toggles[1]) {
			HoldKey(desync_slot + 1);
			ReleaseKey(desync_slot + 1);
			HoldKey(desync_slot + 1);
			ReleaseKey(desync_slot + 1);
		}
	}
}

static void Speedglitchloop()
{
    int sleep1 = 16, sleep2 = 16;
    int last_fps = 0;
    const float EPSILON = 0.008f; // Small value to account for floating-point imprecision
    
    while (true) {
        if (last_fps != RobloxFPS.load(std::memory_order_relaxed)) {
            float delay_float = 1000.0f / RobloxFPS.load(std::memory_order_relaxed);
            int delay_floor = static_cast<int>(delay_float);
            int delay_ceil = delay_floor + 1;
            float fractional = delay_float - delay_floor;

            // More robust floating-point comparisons
            if (fractional < 0.33f - EPSILON) {
                sleep1 = sleep2 = delay_floor;
            }
            else if (fractional > 0.66f + EPSILON) {
                sleep1 = sleep2 = delay_ceil;
            }
            else {
                sleep1 = delay_floor;
                sleep2 = delay_ceil;
            }

            last_fps = RobloxFPS.load(std::memory_order_relaxed);
        }
        
        while (!isspeed.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        if (macrotoggled && notbinding && section_toggles[3]) {
            MoveMouse(speed_strengthx, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep1));
            MoveMouse(speed_strengthy, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep2));
        }
    }
}


static void SpeedglitchloopHHJ()
{
    int sleep1 = 16, sleep2 = 16;
    int last_fps = 0;
    const float EPSILON = 0.008f;
    
    while (true) {
        if (last_fps != RobloxFPS.load(std::memory_order_relaxed)) {
            float delay_float = 1000.0f / RobloxFPS.load(std::memory_order_relaxed);
            int delay_floor = static_cast<int>(delay_float);
            int delay_ceil = delay_floor + 1;
            float fractional = delay_float - delay_floor;

            if (fractional < 0.33f - EPSILON) {
                sleep1 = sleep2 = delay_floor;
            }
            else if (fractional > 0.66f + EPSILON) {
                sleep1 = sleep2 = delay_ceil;
            }
            else {
                sleep1 = delay_floor;
                sleep2 = delay_ceil;
            }

            last_fps = RobloxFPS.load(std::memory_order_relaxed);
        }
        
        while (!isHHJ.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        if (macrotoggled && notbinding && section_toggles[2]) {
            MoveMouse(speed_strengthx, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep1));
            MoveMouse(speed_strengthy, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep2));
        }
    }
}

static void SpamKeyLoop()
{
	while (true) {
		while (!isspamloop) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		if (macrotoggled && notbinding && section_toggles[11]) {
			HoldKeyBinded(vk_spamkey);
			std::this_thread::sleep_for(std::chrono::milliseconds(real_delay));
			ReleaseKeyBinded(vk_spamkey);
			std::this_thread::sleep_for(std::chrono::milliseconds(real_delay));
		}
	}
}

static void ItemClipLoop()
{
	while (true) {
		while (!isitemloop) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		if (macrotoggled && notbinding && section_toggles[8]) {
			HoldKey(clip_slot + 1);
			std::this_thread::sleep_for(std::chrono::milliseconds(clip_delay / 2));
			ReleaseKey(clip_slot + 1);
			std::this_thread::sleep_for(std::chrono::milliseconds(clip_delay / 2));
		}
	}
}

static void WallWalkLoop()
{
	int delay = 16;
	while (true) {
		while (!iswallwalkloop) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			delay = static_cast<unsigned int>(((1000.0f / RobloxFPS.load(std::memory_order_relaxed)) + .5) * 1.1);
		}
		if (macrotoggled && notbinding && section_toggles[10]) {
			if (wallwalktoggleside) {
				MoveMouse(-wallwalk_strengthx, 0);
				std::this_thread::sleep_for(std::chrono::milliseconds(delay));
				MoveMouse(-wallwalk_strengthy, 0);
				std::this_thread::sleep_for(std::chrono::microseconds(RobloxWallWalkValueDelay));
			} else {
				MoveMouse(wallwalk_strengthx, 0);
				std::this_thread::sleep_for(std::chrono::milliseconds(delay));
				MoveMouse(wallwalk_strengthy, 0);
				std::this_thread::sleep_for(std::chrono::microseconds(RobloxWallWalkValueDelay));
			}
		}
	}
}

static void BhopLoop()
{
    while (true) {
        while (!isbhoploop.load(std::memory_order_acquire)) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        HoldKeyBinded(vk_bunnyhopkey);
        std::this_thread::sleep_for(std::chrono::milliseconds(BunnyHopDelay / 2));
        ReleaseKeyBinded(vk_bunnyhopkey);
        std::this_thread::sleep_for(std::chrono::milliseconds(BunnyHopDelay / 2));
    }
}

static void WallhopThread() {
    while (true) {
		while (!iswallhopthread.load(std::memory_order_acquire)) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		if (wallhopswitch) {
			MoveMouse(-wallhop_dx, 0);
		} else {
			MoveMouse(wallhop_dx, 0);
		}

		if (toggle_flick) {
			if (WallhopBonusDelay > 0 && WallhopBonusDelay < WallhopDelay) {
				std::this_thread::sleep_for(std::chrono::milliseconds(WallhopBonusDelay));

				if (toggle_jump) {
					HoldKey(0x39);
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(WallhopDelay - WallhopBonusDelay));
			} else {
				if (toggle_jump) {
					HoldKey(0x39);
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(WallhopDelay));
			}

			if (wallhopswitch) {
				MoveMouse(-wallhop_dy, 0);
			} else {
				MoveMouse(wallhop_dy, 0);
			}
		} else {
			if (toggle_jump) {
				HoldKey(0x39);
			}
		}

		if (toggle_jump) {
            if (100 - WallhopDelay > 0) {
			    std::this_thread::sleep_for(std::chrono::milliseconds(100 - WallhopDelay));
            }
			ReleaseKey(0x39);
		}

		iswallhopthread = false;
	}
}

static void PressKeyThread() {
    while (true) {
		while (!ispresskeythread.load(std::memory_order_relaxed)) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

		if (vk_zkey == vk_dkey) {
			ReleaseKeyBinded(vk_zkey);
		}

		// Sleep for the bonus delay if applicable
		if (PressKeyBonusDelay != 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(PressKeyBonusDelay));
		}

		HoldKeyBinded(vk_dkey);
		std::this_thread::sleep_for(std::chrono::milliseconds(PressKeyDelay));
		ReleaseKeyBinded(vk_dkey);

		ispresskeythread = false;
    }
}

static bool IsMainWindow(HWND hwnd)
{
	return (IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == NULL);
}

static std::vector<HWND> FindWindowByProcessHandle(const std::vector<HANDLE> &handles)
{
    std::vector<HWND> windows;
    for (HANDLE hProcess : handles) {
        DWORD targetPID = GetProcessId(hProcess);
        HWND rbxhwnd = FindWindowEx(NULL, NULL, NULL, NULL);
        while (rbxhwnd != NULL) {
            DWORD windowPID = 0;
            GetWindowThreadProcessId(rbxhwnd, &windowPID);
            if (windowPID == targetPID && IsMainWindow(rbxhwnd)) {
                windows.push_back(rbxhwnd);
                break; // Assume one main window per process
            }
            rbxhwnd = FindWindowEx(NULL, rbxhwnd, NULL, NULL);
        }
    }
    return windows;
}

static HWND FindNewestProcessWindow(const std::vector<HWND> &hwnds)
{
    if (hwnds.empty()) {
        return NULL;
    }

    DWORD newestPID = 0;
    ULONGLONG newestCreationTime = 0;
    HWND newestHWND = NULL;
    bool foundAny = false;

    for (HWND hwnd : hwnds) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);

        // Open the process to get its creation time
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess) {
            FILETIME ftCreation, ftExit, ftKernel, ftUser;
            if (GetProcessTimes(hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser)) {
                ULONGLONG creationTime = (static_cast<ULONGLONG>(ftCreation.dwHighDateTime) << 32) | ftCreation.dwLowDateTime;
                if ((!foundAny || creationTime > newestCreationTime) && IsMainWindow(hwnd)) {
                    newestCreationTime = creationTime;
                    newestPID = pid;
                    newestHWND = hwnd;
                    foundAny = true;
                }
            }
            CloseHandle(hProcess);
        }
    }

    return foundAny ? newestHWND : NULL;
}

static bool IsForegroundWindowProcess(const std::vector<HANDLE> &handles)
{
    if (g_isLinuxWine) {
		return true;
	}

    HWND foreground = GetForegroundWindow();
    if (!foreground) return false; // No foreground window

    DWORD foregroundPID = 0;
    GetWindowThreadProcessId(foreground, &foregroundPID);

    for (HANDLE hProcess : handles) {
        DWORD targetPID = GetProcessId(hProcess);
        if (foregroundPID == targetPID) {
            return true; // Found a match
        }
    }
    return false; // No match
}

static void PasteText(const std::string &text)
{
	if (g_isLinuxWine) {
		// Always use compat version for Linux/Wine
		for (char c : text) {
			KeyAction action = CharToKeyAction_Compat(c);

			if (action.vk_code == 0)
				continue; // Skip unmappable characters

			if (action.needs_shift) {
				HoldKeyBinded(VK_SHIFT);
			}

			HoldKeyBinded(action.vk_code);
			std::this_thread::sleep_for(std::chrono::milliseconds(PasteDelay));
			ReleaseKeyBinded(action.vk_code);

			if (action.needs_shift) {
				ReleaseKeyBinded(VK_SHIFT);
			}
		}
	} else if (useoldpaste) {
		// Windows with old paste: Use Unicode method
		INPUT input = {0};
		input.type = INPUT_KEYBOARD;
		input.ki.wVk = 0;
		input.ki.dwFlags = KEYEVENTF_UNICODE;

		for (char c : text) {
			// Windows can handle many characters directly with wScan
			input.ki.wScan = c;
			input.ki.dwFlags = KEYEVENTF_UNICODE;
			SendInput(1, &input, sizeof(INPUT));
			input.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
			SendInput(1, &input, sizeof(INPUT));
			std::this_thread::sleep_for(std::chrono::milliseconds(PasteDelay));
		}
	} else {
		// Windows with new paste: Use global scan code method
		for (char c : text) {
			KeyAction action = CharToKeyAction_Global(c);

			if (action.vk_code == 0)
				continue; // Skip unmappable characters

			if (action.needs_shift) {
				HoldKeyBinded(VK_SHIFT); // Keep using VK_SHIFT for shift
			}

			bool extended = (action.scan_code & 0xE000) != 0;
			WORD base_scan_code = action.scan_code & 0x00FF; // Remove extended flag

			HoldKey(base_scan_code, extended);
			std::this_thread::sleep_for(std::chrono::milliseconds(PasteDelay));
			ReleaseKey(base_scan_code, extended);

			if (action.needs_shift) {
				ReleaseKeyBinded(VK_SHIFT);
			}
		}
	}
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true; // Forward ImGUI-related messages

    switch (msg) {
    case WM_INPUT: {
	    g_mouseWheel.ProcessRawInput(lParam);
	    break;
    }
    case WM_SIZE: {
	    // Get window size
	    GetWindowRect(hWnd, &screen_rect);

	    screen_width = screen_rect.right - screen_rect.left;
	    screen_height = screen_rect.bottom - screen_rect.top;

	    if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
		    CleanupRenderTarget();
		    g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam),
						DXGI_FORMAT_UNKNOWN, 0);
		    CreateRenderTarget();
	    }
	    // Update raw screen rects
	    RECT raw_screen_rect;
	    GetWindowRect(hwnd, &raw_screen_rect);
	    raw_window_width = raw_screen_rect.right - raw_screen_rect.left;
	    raw_window_height = raw_screen_rect.bottom - raw_screen_rect.top;
	    return 0;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);

        // Set the minimum screen size
        mmi->ptMinTrackSize.x = 1147;
        mmi->ptMinTrackSize.y = 780;
        return 0;
    }
    case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
    case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
    case WM_CLOSE:
		done = true;
		PostQuitMessage(0);
		return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

bool CreateDeviceD3D(HWND hWnd)
{
    RECT rect;
    GetClientRect(hWnd, &rect);
    int window_width = rect.right - rect.left;
    int window_height = rect.bottom - rect.top;
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2; // Use double buffering
    sd.BufferDesc.Width = window_width;
    sd.BufferDesc.Height = window_height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 100;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    HRESULT res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, NULL, &g_pd3dDeviceContext);

    if (FAILED(res))
		return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) {
		g_pSwapChain->Release();
		g_pSwapChain = NULL;
    }
    if (g_pd3dDeviceContext) {
		g_pd3dDeviceContext->Release();
		g_pd3dDeviceContext = NULL;
    }
    if (g_pd3dDevice) {
		g_pd3dDevice->Release();
		g_pd3dDevice = NULL;
    }
}

void CreateRenderTarget()
{
    ID3D11Texture2D *pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) {
		g_mainRenderTargetView->Release();
		g_mainRenderTargetView = NULL;
    }
}

static std::string TrimNullChars(const char *buffer, size_t size)
{
    size_t length = std::strlen(buffer);
    if (length > size)
		length = size;
    return std::string(buffer, length);
}

using NumericVar = std::variant<int*, float*, unsigned int*>;

// Boolean variables to save
const std::unordered_map<std::string, bool *> bool_vars = {
	{"macrotoggled", &macrotoggled},
	{"shiftswitch", &shiftswitch},
	{"wallhopswitch", &wallhopswitch},
	{"wallhopcamfix", &wallhopcamfix},
	{"unequiptoggle", &unequiptoggle},
	{"isspeedswitch", &isspeedswitch},
	{"isfreezeswitch", &isfreezeswitch},
	{"iswallwalkswitch", &iswallwalkswitch},
	{"isspamswitch", &isspamswitch},
	{"isitemclipswitch", &isitemclipswitch},
	{"autotoggle", &autotoggle},
	{"toggle_jump", &toggle_jump},
	{"toggle_flick", &toggle_flick},
	{"camfixtoggle", &camfixtoggle},
	{"wallwalktoggleside", &wallwalktoggleside},
	{"antiafktoggle", &antiafktoggle},
	{"fasthhj", &fasthhj},
	{"hhjzoomin", &hhjzoomin},
	{"hhjzoominreverse", &hhjzoominreverse},
	{"laughzoomin", &laughzoomin},
	{"laughzoominreverse", &laughzoominreverse},
	{"lhjzoomin", &lhjzoomin},
	{"lhjzoominreverse", &lhjzoominreverse},
	{"wallesslhjswitch", &wallesslhjswitch},
	{"chatoverride", &chatoverride},
	{"bounceautohold", &bounceautohold},
	{"bouncerealignsideways", &bouncerealignsideways},
	{"bouncesidetoggle", &bouncesidetoggle},
	{"laughmoveswitch", &laughmoveswitch},
	{"freezeoutsideroblox", &freezeoutsideroblox},
	{"takeallprocessids", &takeallprocessids},
	{"ontoptoggle", &ontoptoggle},
	{"bunnyhopsmart", &bunnyhopsmart},
	{"presskeyinroblox", &presskeyinroblox},
	{"unequipinroblox", &unequipinroblox},
	{"doublepressafkkey", &doublepressafkkey},
	{"useoldpaste", &useoldpaste},
};

// Numeric variables to save
const std::unordered_map<std::string, NumericVar> numeric_vars = {
	{"vk_f5", &vk_f5},
	{"vk_f6", &vk_f6},
	{"vk_f8", &vk_f8},
	{"vk_mbutton", &vk_mbutton},
	{"vk_xbutton1", &vk_xbutton1},
	{"vk_xbutton2", &vk_xbutton2},
	{"vk_leftbracket", &vk_leftbracket},
	{"vk_spamkey", &vk_spamkey},
	{"vk_zkey", &vk_zkey},
	{"vk_dkey", &vk_dkey},
	{"vk_xkey", &vk_xkey},
	{"vk_clipkey", &vk_clipkey},
	{"vk_laughkey", &vk_laughkey},
	{"vk_bouncekey", &vk_bouncekey},
	{"vk_bunnyhopkey", &vk_bunnyhopkey},
	{"vk_shiftkey", &vk_shiftkey},
	{"vk_enterkey", &vk_enterkey},
	{"vk_chatkey", &vk_chatkey},
	{"vk_afkkey", &vk_afkkey},
	{"selected_dropdown", &selected_dropdown},
	{"vk_wallkey", &vk_wallkey},
	{"PreviousWallWalkSide", &PreviousWallWalkSide},
	{"speed_slot", &speed_slot},
	{"desync_slot", &desync_slot},
	{"clip_slot", &clip_slot},
	{"spam_delay", &spam_delay},
	{"real_delay", &real_delay},
	{"wallhop_dx", &wallhop_dx},
	{"wallhop_dy", &wallhop_dy},
	{"PreviousWallWalkValue", &PreviousWallWalkValue},
	{"maxfreezetime", &maxfreezetime},
	{"maxfreezeoverride", &maxfreezeoverride},
	{"RobloxWallWalkValueDelay", &RobloxWallWalkValueDelay},
	{"speed_strengthx", &speed_strengthx},
	{"speedoffsetx", &speedoffsetx},
	{"speed_strengthy", &speed_strengthy},
	{"speedoffsety", &speedoffsety},
	{"clip_delay", &clip_delay},
	{"RobloxPixelValue", &RobloxPixelValue},
	{"PreviousSensValue", &PreviousSensValue},
	{"windowOpacityPercent", &windowOpacityPercent},
	{"AntiAFKTime", &AntiAFKTime},
	{"display_scale", &display_scale},
	{"WindowPosX", &WindowPosX},
	{"WindowPosY", &WindowPosY},
};

// Char variables to save
const std::vector<std::pair<std::string, std::pair<char*, size_t>>> char_arrays = {
    {"settingsBuffer", {settingsBuffer, sizeof(settingsBuffer)}},
    {"ItemDesyncSlot", {ItemDesyncSlot, sizeof(ItemDesyncSlot)}},
    {"ItemSpeedSlot", {ItemSpeedSlot, sizeof(ItemSpeedSlot)}},
    {"ItemClipSlot", {ItemClipSlot, sizeof(ItemClipSlot)}},
    {"ItemClipDelay", {ItemClipDelay, sizeof(ItemClipDelay)}},
	{"BunnyHopDelayChar", {BunnyHopDelayChar, sizeof(BunnyHopDelayChar)}},
    {"RobloxSensValue", {RobloxSensValue, sizeof(RobloxSensValue)}},
    {"RobloxWallWalkValueChar", {RobloxWallWalkValueChar, sizeof(RobloxWallWalkValueChar)}},
    {"RobloxWallWalkValueDelayChar", {RobloxWallWalkValueDelayChar, sizeof(RobloxWallWalkValueDelayChar)}},
    {"WallhopPixels", {WallhopPixels, sizeof(WallhopPixels)}},
    {"SpamDelay", {SpamDelay, sizeof(SpamDelay)}},
    {"RobloxPixelValueChar", {RobloxPixelValueChar, sizeof(RobloxPixelValueChar)}},
    {"CustomTextChar", {CustomTextChar, sizeof(CustomTextChar)}},
	{"RobloxFPSChar", {RobloxFPSChar, sizeof(RobloxFPSChar)}},
	{"AntiAFKTimeChar", {AntiAFKTimeChar, sizeof(AntiAFKTimeChar)}},
	{"WallhopDelayChar", {WallhopDelayChar, sizeof(WallhopDelayChar)}},
	{"WallhopBonusDelayChar", {WallhopBonusDelayChar, sizeof(WallhopBonusDelayChar)}},
	{"PressKeyDelayChar", {PressKeyDelayChar, sizeof(PressKeyDelayChar)}},
	{"PressKeyBonusDelayChar", {PressKeyBonusDelayChar, sizeof(PressKeyBonusDelayChar)}},
	{"PasteDelayChar", {PasteDelayChar, sizeof(PasteDelayChar)}},
	{"HHJLengthChar", {HHJLengthChar, sizeof(HHJLengthChar)}},
	{"HHJFreezeDelayOverrideChar", {HHJFreezeDelayOverrideChar, sizeof(HHJFreezeDelayOverrideChar)}},
	{"HHJDelay1Char", {HHJDelay1Char, sizeof(HHJDelay1Char)}},
	{"HHJDelay2Char", {HHJDelay2Char, sizeof(HHJDelay2Char)}},
	{"HHJDelay3Char", {HHJDelay3Char, sizeof(HHJDelay3Char)}},
};

void SaveSettings(const std::string& filepath, const std::string& profile_name) {
    json current_profile_data; // JSON object for the settings of the current profile

	if (profile_name.empty() || profile_name == "(default)") {
		return;
    }

	std::string profile_name_modified = profile_name;

	// Secret code to save a default profile
	if (profile_name == "SAVE_DEFAULT_90493") {
		profile_name_modified = "(default)";
	}

    // Save booleans
    for (const auto& [key, ptr] : bool_vars) {
        if (ptr) current_profile_data[key] = *ptr;
    }

    // Save numerics
    for (const auto& [key, var] : numeric_vars) {
        std::visit([&](auto&& arg) {
            if (arg) current_profile_data[key] = *arg;
        }, var);
    }

    // Save char arrays
    for (const auto& [key, cfg] : char_arrays) {
        if (cfg.first) current_profile_data[key] = TrimNullChars(cfg.first, cfg.second);
    }

    // Special cases
    if (section_amounts > 0) {
        current_profile_data["section_toggles"] = std::vector<bool>(section_toggles, section_toggles + section_amounts);
        current_profile_data["section_order_vector"] = std::vector<int>(section_order, section_order + section_amounts);
    }

    current_profile_data["text"] = text;
    current_profile_data["screen_width"] = screen_width;
    current_profile_data["screen_height"] = screen_height;

    // --- Profile Handling ---
    json root_json_output = json::object(); // This will be written to the file

    std::ifstream infile(filepath);
    if (infile.is_open()) {
        try {
            json existing_file_data;
            infile >> existing_file_data;
            infile.close();

            if (existing_file_data.is_object() && !existing_file_data.empty()) {
                bool is_already_profile_format = false;
                // Check if any top-level key looks like a profile name (e.g., starts with "Profile ")
                // This is a heuristic; adjust if your profile names have a different pattern.
                for (const auto& item : existing_file_data.items()) {
                    if (item.key().rfind("(default)", 0) == 0 && item.value().is_object()) {
                        is_already_profile_format = true;
                        break;
                    }
                }

                if (is_already_profile_format) {
                    root_json_output = existing_file_data; // It's already in the new profile format
                } else {
                    // It's an object, but not in "Profile X" format.
                    // Assume it's an old flat format if it contains recognizable settings.
                    // This check helps differentiate an old config from an empty or unrelated JSON object.
                    if ((!bool_vars.empty() && !bool_vars.begin()->first.empty() && existing_file_data.contains(bool_vars.begin()->first)) ||
                        (!numeric_vars.empty() && !numeric_vars.begin()->first.empty() && existing_file_data.contains(numeric_vars.begin()->first)))
                    {
                        std::cout << "Old format detected during save. Converting. Old data will be under 'Profile 1'." << std::endl;
                        root_json_output["Profile 1"] = existing_file_data; // Convert old data to "Profile 1"
                    } else {
                        if (!existing_file_data.empty()) {
                             std::cerr << "Warning: Existing settings file '" << filepath << "' contains an unrecognized JSON object structure. "
                                       << "It will be treated as a base for new profile data." << std::endl;
                             root_json_output = existing_file_data; // Keep it if it was some other map
                        }
                        // If existing_file_data was empty, root_json_output is already a new empty object.
                    }
                }
            }
            // If file was empty or not a JSON object, existing_file_data might be null or non-object.
            // In such cases, root_json_output remains a fresh json::object().
        } catch (const json::parse_error& e) {
            std::cerr << "JSON parse error reading '" << filepath << "' for save: " << e.what() << ". Starting with a new profile structure." << std::endl;
            infile.close(); // Ensure file is closed on error
            // root_json_output will be a fresh json::object()
        }
    }
    // If file didn't exist, infile.is_open() was false, root_json_output is a fresh json::object().

    // Add/update the current profile's settings into the root JSON
    root_json_output[profile_name_modified] = current_profile_data;

    if (!G_CURRENTLY_LOADED_PROFILE_NAME.empty()) {
        if (!root_json_output.contains(METADATA_KEY) || !root_json_output[METADATA_KEY].is_object()) {
            root_json_output[METADATA_KEY] = json::object(); // Create metadata object
        }
        root_json_output[METADATA_KEY][LAST_ACTIVE_PROFILE_KEY] = G_CURRENTLY_LOADED_PROFILE_NAME;
    }

	if (profile_name != "SAVE_DEFAULT_90493") {
		// Global variables that don't change across profiles go here saved when not saving as (default)
		root_json_output[METADATA_KEY]["shortdescriptions"] = shortdescriptions;
	}

    // Write the root JSON (which now contains all profiles) to file
    std::ofstream outfile(filepath);
    if (outfile.is_open()) {
        outfile << root_json_output.dump(4); // pretty print with 4 spaces
        outfile.close();
    } else {
        std::cerr << "Error: Could not open settings file for writing: " << filepath << std::endl;
    }
}

void LoadSettings(const std::string& filepath, const std::string& profile_name) {

	if (profile_name == "") {
		return;
	}

	std::ifstream file;
	bool fileFound = false;
	std::filesystem::path real_filepath;

	// First try current directory
	real_filepath = std::filesystem::current_path() / filepath;
	file.open(real_filepath);
	if (file.is_open()) {
		fileFound = true;
	}
	else {
		// If not found, try parent directory
		std::filesystem::path current_path = std::filesystem::current_path();
		if (current_path.has_parent_path()) {
			real_filepath = current_path.parent_path() / filepath;
			file.open(real_filepath);
			if (file.is_open()) {
				fileFound = true;
			}
		}
	}

	if (!fileFound) {
		std::cerr << "Info: Settings file '" << filepath 
				 << "' not found in current or parent directory. Using default values. Saving profile 1." << std::endl;
		return;
	}

	json root_file_json;
	try {
		file >> root_file_json;
		file.close();
	} catch (const json::parse_error& e) {
		std::cerr << "JSON parse error in '" << real_filepath.string() << "': " << e.what() << std::endl;
		file.close();
		return;
	}

	// Make default profile if it doesn't exist
    if ((!root_file_json.contains("(default)") && (profile_name == "SAVE_DEFAULT_90493"))) {
        SaveSettings(filepath, "SAVE_DEFAULT_90493");
    }

    json settings_to_load; // This will hold the JSON data for the specific profile
    bool profile_data_extracted = false;

    // Try to load as new format (profile-based)
    if (root_file_json.is_object() && root_file_json.contains(profile_name)) {
        if (root_file_json.at(profile_name).is_object()) {
            settings_to_load = root_file_json.at(profile_name);
            profile_data_extracted = true;
        } else {
            std::cerr << "Warning: Profile '" << profile_name << "' in '" << filepath << "' is not a valid settings object." << std::endl;
        }
    }

    // Backwards Compatibility: If profile wasn't found AND we are trying to load the designated "legacy" profile name
    if (!profile_data_extracted && profile_name == "Profile 1") { // "Profile 1" is the designated default for old files
        bool looks_like_multi_profile_file = false;
        if (root_file_json.is_object()) {
            for (const auto& item : root_file_json.items()) {
                // Heuristic: if any key starts with "Profile " and its value is an object.
                if (item.key().rfind("Profile ", 0) == 0 && item.value().is_object()) {
                    looks_like_multi_profile_file = true;
                    break;
                }
            }
        }

        // If it doesn't look like a multi-profile file and it contains recognizable settings keys at the root,
        // then treat the root_file_json itself as the settings for "Profile 1".
        if (!looks_like_multi_profile_file && root_file_json.is_object() &&
            (root_file_json.contains("text") || // A common key from old format
             (!bool_vars.empty() && !bool_vars.begin()->first.empty() && root_file_json.contains(bool_vars.begin()->first)) ||
             (!numeric_vars.empty() && !numeric_vars.begin()->first.empty() && root_file_json.contains(numeric_vars.begin()->first))
            ))
        {
            settings_to_load = root_file_json;
            profile_data_extracted = true;
            std::cout << "Info: Loaded legacy format file as '" << profile_name << "'." << std::endl;
        }
    }

    if (!profile_data_extracted) {
		if (profile_name != "SAVE_DEFAULT_90493") {
			std::cerr << "Warning: Profile '" << profile_name << "' not found or data is invalid in '" << filepath << "'. Using current/default values." << std::endl;
		}
        return;
    }
    
    // --- Actual loading  ---
    try {
        // Load booleans
        for (const auto& [key, ptr] : bool_vars) {
            if (ptr && settings_to_load.contains(key) && settings_to_load[key].is_boolean()) {
                *ptr = settings_to_load[key].get<bool>();
            }
        }

        // Load numerics
        for (const auto& [key, var] : numeric_vars) {
            if (!settings_to_load.contains(key)) continue;
            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(*arg)>;
                if (arg && settings_to_load[key].is_number()) {
                    *arg = settings_to_load[key].get<T>();
                }
            }, var);
        }

        // Load char arrays
        for (const auto& [key, cfg] : char_arrays) {
            if (cfg.first && settings_to_load.contains(key) && settings_to_load[key].is_string()) {
                std::string str_val = settings_to_load[key].get<std::string>();
                strncpy(cfg.first, str_val.c_str(), cfg.second -1); // Leave space for null terminator
                cfg.first[cfg.second - 1] = '\0'; // Ensure null-termination
            }
        }

        // Load special cases
        if (section_amounts > 0) {
            if (settings_to_load.contains("section_toggles") && settings_to_load["section_toggles"].is_array()) {
                auto toggles = settings_to_load["section_toggles"].get<std::vector<bool>>();
                size_t count = std::min(toggles.size(), static_cast<size_t>(section_amounts));
                std::copy(toggles.begin(), toggles.begin() + count, section_toggles);
            }

            if (settings_to_load.contains("section_order_vector") && settings_to_load["section_order_vector"].is_array()) {
                auto order = settings_to_load["section_order_vector"].get<std::vector<int>>();
                
                // HACKY SOLUTION TO ADD IN FUNCTIONS IN CUSTOM POSITONS BY DEFAULT!
				// Add in Bunnyhop Location to older save files
                if (std::find(order.begin(), order.end(), 13) == order.end() && order.size() >= 6) {
                     if (order.size() >= 7) {
                         order.insert(order.begin() + 7, 13); // Insert at index 6 (7th element)
                     }
				}
        
                size_t count = std::min(order.size(), static_cast<size_t>(section_amounts));
                for (size_t i = 0; i < count; ++i) {
                    section_order[i] = order[i];
                }
            }
        }

        if (settings_to_load.contains("text") && settings_to_load["text"].is_string()) {
            text = settings_to_load["text"].get<std::string>();
        }
        if (settings_to_load.contains("screen_width") && settings_to_load["screen_width"].is_number_integer()) {
            screen_width = settings_to_load.value("screen_width", screen_width);
        }
        if (settings_to_load.contains("screen_height") && settings_to_load["screen_height"].is_number_integer()) {
            screen_height = settings_to_load.value("screen_height", screen_height);
        }

		if (settings_to_load.contains("RobloxFPSChar") && settings_to_load["RobloxFPSChar"].is_string()) {
            RobloxFPS = std::stoi(RobloxFPSChar);
        }

		// Load global variables in metadata (applies across all profiles)
		if (root_file_json.contains(METADATA_KEY) && root_file_json[METADATA_KEY].is_object()) {
			const auto& metadata = root_file_json[METADATA_KEY];

			if (metadata.contains("shortdescriptions") && metadata["shortdescriptions"].is_boolean()) {
				shortdescriptions = metadata["shortdescriptions"].get<bool>();
			}
		}

    } catch (const json::exception& e) {
        std::cerr << "Load error processing profile '" << profile_name << "': " << e.what() << '\n';
    }
}

std::vector<std::string> GetProfileNames(const std::string& filepath) {
    std::vector<std::string> names;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return names;
    }
    json root_json;
    try {
        file >> root_json;
        if (root_json.is_object()) {
            for (auto& el : root_json.items()) {
				// Skip Metadata
				if (el.key() == METADATA_KEY) {
                    continue;
				}

                if (el.value().is_object()) { // Is it a profile
                    names.push_back(el.key());
                }
            }
        }
    } catch (const json::parse_error& e) {
        std::cerr << "GetProfileNames: JSON parse error in '" << filepath << "': " << e.what() << std::endl;
    }
    file.close();
    // Sort the names alphabetically
    std::sort(names.begin(), names.end());

    // Check if "default" exists in the names
    auto it = std::find(names.begin(), names.end(), "(default)");
    if (it != names.end()) {
        // Move "default" to the front
        names.erase(it);
        names.insert(names.begin(), "(default)");
    }

    return names;
}

bool WriteJsonToFile(const std::string& filepath, const json& data) {
    std::ofstream outfile(filepath);
    if (outfile.is_open()) {
        outfile << data.dump(4);
        outfile.close();
        return true;
    }
    std::cerr << "Error: Could not open file for writing: " << filepath << std::endl;
    return false;
}

bool DeleteProfileFromFile(const std::string& filepath, const std::string& profile_name) {
    std::ifstream infile(filepath);
    if (!infile.is_open()) return false;
    json root_json;
    try { infile >> root_json; } catch (const json::parse_error&) { infile.close(); return false; }
    infile.close();

	if (profile_name == "(default)") {
		return false;
	}

    if (root_json.is_object() && root_json.contains(profile_name)) {
        root_json.erase(profile_name);
        if (WriteJsonToFile(filepath, root_json)) {
            if (G_CURRENTLY_LOADED_PROFILE_NAME == profile_name) {
                G_CURRENTLY_LOADED_PROFILE_NAME = "";
                std::cout << "Info: Deleted profile '" << profile_name << "' was active." << std::endl;
            }
            return true;
        }
    }
    return false;
}

bool RenameProfileInFile(const std::string& filepath, const std::string& old_name, const std::string& new_name) {
    if (old_name == new_name) return true;
    std::ifstream infile(filepath);
    if (!infile.is_open()) return false;
    json root_json;
    try { infile >> root_json; } catch (const json::parse_error&) { infile.close(); return false; }
    infile.close();

	if (old_name == "(default)" || new_name == "(default)") {
		return false;
	}

    if (root_json.is_object() && root_json.contains(old_name)) {
        if (root_json.contains(new_name)) {
             std::cerr << "Rename Error: Target name '" << new_name << "' already exists in file." << std::endl;
            return false;
        }
		
        json profile_data = root_json[old_name];
        root_json.erase(old_name);
        root_json[new_name] = profile_data;
        if (WriteJsonToFile(filepath, root_json)) {
            if (G_CURRENTLY_LOADED_PROFILE_NAME == old_name) {
                G_CURRENTLY_LOADED_PROFILE_NAME = new_name;
            }
            return true;
        }
    }
    return false;
}

bool DuplicateProfileInFile(const std::string& filepath, const std::string& source_name, const std::string& new_name) {
    std::ifstream infile(filepath);
    if (!infile.is_open()) return false;
    json root_json;
    try { infile >> root_json; } catch (const json::parse_error&) { infile.close(); return false; }
    infile.close();

    if (root_json.is_object() && root_json.contains(source_name)) {
        if (root_json.contains(new_name)) {
            std::cerr << "Duplicate Error: Target name '" << new_name << "' already exists in file." << std::endl;
            return false;
        }
        json profile_data_copy = root_json[source_name];
        root_json[new_name] = profile_data_copy;
        return WriteJsonToFile(filepath, root_json);
    }
    return false;
}

std::string GenerateUniqueProfileName(const std::string& base_name, const std::vector<std::string>& existing_names) {
    std::string candidate_name = base_name + " (Copy)";
    if (std::find(existing_names.begin(), existing_names.end(), candidate_name) == existing_names.end()) {
        return candidate_name;
    }
    int i = 2;
    while (true) {
        candidate_name = base_name + " (Copy " + std::to_string(i) + ")";
        if (std::find(existing_names.begin(), existing_names.end(), candidate_name) == existing_names.end()) {
            return candidate_name;
        }
        i++;
        if (i > 1000) return base_name + " (Copy Error)"; // Safety break
    }
}

std::string GenerateNewDefaultProfileName(const std::vector<std::string>& existing_names) {
    int i = 1;
    while (true) {
        std::string candidate_name = "Profile " + std::to_string(i);
        if (std::find(existing_names.begin(), existing_names.end(), candidate_name) == existing_names.end()) {
            return candidate_name;
        }
        i++;
        if (i > 1000) return "Profile Error"; // Safety break
    }
}

bool TryLoadLastActiveProfile(const std::string& filepath) {

    std::ifstream file;
	bool fileFound = false;
	std::filesystem::path real_filepath;

	// First try current directory
	real_filepath = std::filesystem::current_path() / filepath;
	file.open(real_filepath);
	if (file.is_open()) {
		fileFound = true;
	}
	else {
		// If not found, try parent directory
		std::filesystem::path current_path = std::filesystem::current_path();
		if (current_path.has_parent_path()) {
			real_filepath = current_path.parent_path() / filepath;
			file.open(real_filepath);
			if (file.is_open()) {
				fileFound = true;
				LoadSettings(real_filepath.string(), "(default)");
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
				G_CURRENTLY_LOADED_PROFILE_NAME = "Profile 1";
				SaveSettings(filepath, "Profile 1");

				std::cerr << "Found Parent Save File!" << std::endl;
				return true;
			}
		}
	}

    if (!fileFound) {
        std::cerr << "TryLoadLastActiveProfile: Settings file '" << filepath << "' not found." << std::endl;
        return false;
    }

    json root_json;
    try {
        file >> root_json;
        file.close();
    } catch (const json::parse_error& e) {
        std::cerr << "TryLoadLastActiveProfile: JSON parse error in '" << filepath << "': " << e.what() << std::endl;
        file.close();
        return false;
    }

    if (root_json.is_object() && root_json.contains(METADATA_KEY) && root_json[METADATA_KEY].is_object()) {
        const auto& metadata = root_json[METADATA_KEY];
        if (metadata.contains(LAST_ACTIVE_PROFILE_KEY) && metadata[LAST_ACTIVE_PROFILE_KEY].is_string()) {
            std::string last_active_name = metadata[LAST_ACTIVE_PROFILE_KEY].get<std::string>();

            // Check if this profile actually exists in the file
            if (!last_active_name.empty() && root_json.contains(last_active_name) && root_json[last_active_name].is_object()) {
                LoadSettings(filepath, last_active_name); 
                G_CURRENTLY_LOADED_PROFILE_NAME = last_active_name;
                std::cout << "Successfully loaded last active profile: " << last_active_name << std::endl;
                return true;
            } else {
                std::cerr << "TryLoadLastActiveProfile: Last active profile '" << last_active_name << "' not found or invalid in settings file." << std::endl;
            }
        } else {
            // std::cout << "TryLoadLastActiveProfile: No 'last_active_profile' key found in metadata." << std::endl;
        }
    } else {
		// Import old settings in
		if (root_json.is_object()) {
			LoadSettings(filepath, "Profile 1");
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			SaveSettings(filepath, "Profile 1");
			G_CURRENTLY_LOADED_PROFILE_NAME = "Profile 1";
		}
    }
    return false;
}

namespace ProfileUI {
    static bool s_expanded = false;
    static std::vector<std::string> s_profile_names;
    static int s_selected_profile_idx = -1;
    
    static int s_editing_profile_idx = -1;
    static char s_edit_buffer[256] = "";
    static float s_last_click_time = -1.0f;
    static int s_last_clicked_item_idx = -1;

    static bool s_profiles_initialized = false;
    static std::string s_rename_error_msg = "";

	// Delete button
	static int s_delete_button_confirmation_stage = 0; // 0: Normal, 1: Yellow (confirm), 2: Red (deleted)
	static float s_delete_button_stage_timer = 0.0f;
	static int s_target_profile_idx_for_confirm = -1;
	static std::string s_G_LOADED_PROFILE_NAME_at_confirm_start = "";
	bool delete_action_requested_this_frame = false;

    void RefreshProfileListAndSelection() {
        std::string previously_selected_name;
        if (s_selected_profile_idx >= 0 && s_selected_profile_idx < s_profile_names.size()) {
            previously_selected_name = s_profile_names[s_selected_profile_idx];
        }
        
        s_profile_names = GetProfileNames(G_SETTINGS_FILEPATH); // Reads and sorts

        s_selected_profile_idx = -1; // Reset selection
        if (!previously_selected_name.empty()) {
            auto it = std::find(s_profile_names.begin(), s_profile_names.end(), previously_selected_name);
            if (it != s_profile_names.end()) {
                s_selected_profile_idx = std::distance(s_profile_names.begin(), it);
            }
        }
        // If nothing was selected, or previous selection is gone, try to select G_CURRENTLY_LOADED_PROFILE_NAME
        if (s_selected_profile_idx == -1 && !G_CURRENTLY_LOADED_PROFILE_NAME.empty()) {
            auto it = std::find(s_profile_names.begin(), s_profile_names.end(), G_CURRENTLY_LOADED_PROFILE_NAME);
            if (it != s_profile_names.end()) {
                s_selected_profile_idx = std::distance(s_profile_names.begin(), it);
            }
        }
    }


    void InitializeProfiles() {
        if (!s_profiles_initialized) {
            RefreshProfileListAndSelection();
            s_profiles_initialized = true;
        }
    }

    void DrawProfileManagerUI() {
        InitializeProfiles();
		ImVec4 current_button_bg_color = ImGui::GetStyle().Colors[ImGuiCol_Button];
		ImVec4 current_button_text_color = ImGui::GetStyle().Colors[ImGuiCol_Text];

        if (ImGui::Button(s_expanded ? "Profiles <" : "Profiles >", ImVec2(270, 0))) {
            s_expanded = !s_expanded;
            if (s_expanded) {
                RefreshProfileListAndSelection(); // Refresh list when opening
                s_rename_error_msg = ""; // Clear previous rename errors
            } else {
                s_editing_profile_idx = -1; // Close editor if dropdown closes
            }
        }


        if (s_expanded) {
            ImVec2 buttonPos = ImGui::GetItemRectMin();
            float menuWidth = ImGui::GetItemRectSize().x;
            
            float list_item_height = ImGui::GetTextLineHeightWithSpacing();
            int num_items_to_show = std::min(10, (int)s_profile_names.size());
            if (num_items_to_show == 0) num_items_to_show = 3; // Min height for empty list
            float buttons_height = ImGui::GetFrameHeightWithSpacing() * 2.0f + ImGui::GetStyle().ItemSpacing.y * 2.0f;
            float list_height = num_items_to_show * list_item_height + ImGui::GetStyle().WindowPadding.y * 2;
            float menuHeight = buttons_height + list_height + ImGui::GetStyle().SeparatorTextAlign.y; // Approx
			int old_s_selected_profile_idx = -1;
            menuHeight = std::min(menuHeight, 300.0f);


            ImGui::SetNextWindowPos(ImVec2(buttonPos.x, buttonPos.y - menuHeight - ImGui::GetStyle().WindowPadding.y + 3));
            ImGui::SetNextWindowSize(ImVec2(menuWidth, menuHeight));

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
            ImGui::Begin("##ProfilesDropUpMenu", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

            float actionButtonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3.0f;
            bool profile_is_selected = (s_selected_profile_idx != -1 && s_selected_profile_idx < s_profile_names.size());

			// Save current profile every time s_selected_profile_idx is updated
			if (old_s_selected_profile_idx != s_selected_profile_idx) {
				SaveSettings(G_SETTINGS_FILEPATH, G_CURRENTLY_LOADED_PROFILE_NAME);
			}

			if (profile_is_selected) {
				old_s_selected_profile_idx = s_selected_profile_idx;
			}

            // Buttons
            if (ImGui::Button("Save To", ImVec2(actionButtonWidth, 0))) {
                std::string profileToSave;

                if (profile_is_selected) {
                    profileToSave = s_profile_names[s_selected_profile_idx];
                } else {
                    profileToSave = GenerateNewDefaultProfileName(s_profile_names);
                }

				G_CURRENTLY_LOADED_PROFILE_NAME = profileToSave;
                SaveSettings(G_SETTINGS_FILEPATH, profileToSave);
                RefreshProfileListAndSelection(); // Update list
                // Ensure the saved profile is selected
                auto it = std::find(s_profile_names.begin(), s_profile_names.end(), profileToSave);
                if (it != s_profile_names.end()) s_selected_profile_idx = std::distance(s_profile_names.begin(), it);

                std::cout << "Saved to profile: " << profileToSave << std::endl;
                s_editing_profile_idx = -1; // Ensure editing stops
            }

            ImGui::SameLine();
            if (!profile_is_selected) ImGui::BeginDisabled();

            if (ImGui::Button("Load", ImVec2(actionButtonWidth, 0))) {
                if (profile_is_selected) {
                    LoadSettings(G_SETTINGS_FILEPATH, s_profile_names[s_selected_profile_idx]);
                    G_CURRENTLY_LOADED_PROFILE_NAME = s_profile_names[s_selected_profile_idx];
                    s_editing_profile_idx = -1;
                }
            }

            if (!profile_is_selected) ImGui::EndDisabled();
            ImGui::SameLine();

            if (!profile_is_selected) ImGui::BeginDisabled();

			ImGuiIO &io = ImGui::GetIO();

			// Manage Delete Color
			if (s_delete_button_confirmation_stage != 0) {
				s_delete_button_stage_timer -= io.DeltaTime;
				bool should_reset_state = false;
				if (s_delete_button_stage_timer <= 0.0f) {
					should_reset_state = true; // Timer expired
				}
				if (s_target_profile_idx_for_confirm != -1 && s_selected_profile_idx != s_target_profile_idx_for_confirm) {
					should_reset_state = true;
				}
				if (!s_G_LOADED_PROFILE_NAME_at_confirm_start.empty() && 
					G_CURRENTLY_LOADED_PROFILE_NAME != s_G_LOADED_PROFILE_NAME_at_confirm_start) {
					should_reset_state = true;
				}
				if (should_reset_state) {
					s_delete_button_confirmation_stage = 0;
					s_target_profile_idx_for_confirm = -1;
					s_G_LOADED_PROFILE_NAME_at_confirm_start = "";
					s_delete_button_stage_timer = 0.0f;
				}
			}

			// Update Delete Button Color
			if (profile_is_selected && s_selected_profile_idx == s_target_profile_idx_for_confirm) {
				if (s_delete_button_confirmation_stage == 1) {
					current_button_bg_color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
					current_button_text_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f); // Black
				}
			}

			ImGui::PushStyleColor(ImGuiCol_Text, current_button_text_color);
			ImGui::PushStyleColor(ImGuiCol_Button, current_button_bg_color);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, current_button_bg_color);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, current_button_bg_color);

            if (ImGui::Button("Delete", ImVec2(actionButtonWidth, 0))) {
                if (profile_is_selected) {
                    delete_action_requested_this_frame = true;
                }
            }
			ImGui::PopStyleColor(4);


			if (delete_action_requested_this_frame) {
				if (profile_is_selected) {
					if (s_delete_button_confirmation_stage == 0 || s_selected_profile_idx != s_target_profile_idx_for_confirm) {
						s_delete_button_confirmation_stage = 1; // Yellow
						s_delete_button_stage_timer = 0.7f;
						s_target_profile_idx_for_confirm = s_selected_profile_idx;
						s_G_LOADED_PROFILE_NAME_at_confirm_start = G_CURRENTLY_LOADED_PROFILE_NAME;
					} else if (s_delete_button_confirmation_stage == 1 && s_selected_profile_idx == s_target_profile_idx_for_confirm) {
						s_delete_button_confirmation_stage = 2;

						// Delete Profile
						std::string name_to_delete = s_profile_names[s_target_profile_idx_for_confirm];
						if (DeleteProfileFromFile(G_SETTINGS_FILEPATH, name_to_delete)) {
							RefreshProfileListAndSelection();
							// Delete your profile if you deleted your last save file
							std::ifstream infile(G_SETTINGS_FILEPATH);

							if (!infile.is_open()) return;
							json root_json;
							try { infile >> root_json; } catch (const json::parse_error&) { infile.close(); return; }
							infile.close();

							if (root_json.is_object() && root_json.size() == 1) {
								std::filesystem::remove(G_SETTINGS_FILEPATH);
							}
						}
						s_editing_profile_idx = -1;
					}
				}
			}
	
			delete_action_requested_this_frame = false;

            if (!profile_is_selected) ImGui::EndDisabled();

            if (!profile_is_selected) ImGui::BeginDisabled();
            if (ImGui::Button("Duplicate", ImVec2(-FLT_MIN, 0))) {
                if (profile_is_selected) {
                    std::string source_name = s_profile_names[s_selected_profile_idx];
                    std::string new_name = GenerateUniqueProfileName(source_name, s_profile_names);
                    if (DuplicateProfileInFile(G_SETTINGS_FILEPATH, source_name, new_name)) {
						// Load New Profile
                        LoadSettings(G_SETTINGS_FILEPATH, new_name);
                        G_CURRENTLY_LOADED_PROFILE_NAME = new_name;
                        RefreshProfileListAndSelection();

                        auto it = std::find(s_profile_names.begin(), s_profile_names.end(), new_name);
                        if (it != s_profile_names.end()) s_selected_profile_idx = std::distance(s_profile_names.begin(), it);

                    }
                    s_editing_profile_idx = -1;
                }
            }
            if (!profile_is_selected) ImGui::EndDisabled();
            ImGui::Separator();

            // --- Scrollable selectable list ---
            ImGui::BeginChild("##ProfilesOptionsList", ImVec2(0, ImGui::GetContentRegionAvail().y - ImGui::GetStyle().ItemSpacing.y - (s_rename_error_msg.empty() ? 0 : ImGui::GetTextLineHeightWithSpacing())) , true, ImGuiWindowFlags_HorizontalScrollbar);
            {
                for (int i = 0; i < s_profile_names.size(); ++i) {
                    bool is_editing_this_item = (s_editing_profile_idx == i);
                    bool is_selected_this_item = (s_selected_profile_idx == i);

                    bool is_currently_loaded = (!G_CURRENTLY_LOADED_PROFILE_NAME.empty() && G_CURRENTLY_LOADED_PROFILE_NAME == s_profile_names[i]);

                    if (is_currently_loaded) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green
                    }

					if (s_profile_names[i] == "(default)") {
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.54f, 0.0f, 1.0f)); // Orange
					}

                    if (is_editing_this_item) {
                        ImGui::SetKeyboardFocusHere();
                        ImGui::SetNextItemWidth(-FLT_MIN); 

                        if ((ImGui::InputText("##EditProfileName", s_edit_buffer, IM_ARRAYSIZE(s_edit_buffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) || (ImGui::IsMouseClicked(0) && !ImGui::IsItemHovered())) {
                            std::string new_name_candidate = s_edit_buffer;
                            s_rename_error_msg = ""; // Clear previous error
                            
							if (new_name_candidate == "(default)") {
								s_rename_error_msg = "Name cannot be default.";
							}

                            if (new_name_candidate.empty()) {
                                s_rename_error_msg = "Name cannot be empty.";
                            } else {
                                bool name_exists_for_another_profile = false;
                                for (int j = 0; j < s_profile_names.size(); ++j) {
                                    if (j != i && s_profile_names[j] == new_name_candidate) {
                                        name_exists_for_another_profile = true;
                                        break;
                                    }
                                }
                                if (name_exists_for_another_profile) {
                                     s_rename_error_msg = "Name already exists.";
                                }
                            }
                            
                            if (s_rename_error_msg.empty()) { // If no error
                                if (RenameProfileInFile(G_SETTINGS_FILEPATH, s_profile_names[i], new_name_candidate)) {
                                    s_profile_names[i] = new_name_candidate; // Update local list immediately
                                    RefreshProfileListAndSelection();
                                    // Re-find the selected index after sort
                                    auto it = std::find(s_profile_names.begin(), s_profile_names.end(), new_name_candidate);
                                    if (it != s_profile_names.end()) s_selected_profile_idx = std::distance(s_profile_names.begin(), it);
                                    
                                    if (G_CURRENTLY_LOADED_PROFILE_NAME == s_profile_names[i] && s_profile_names[i] != new_name_candidate) {
                                        // This case should be handled by RenameProfileInFile updating G_CURRENTLY_LOADED_PROFILE_NAME
                                    }
                                    std::cout << "Renamed profile to: " << new_name_candidate << std::endl;
                                }

                                s_editing_profile_idx = -1; // Exit editing mode on success or file failure
                            }
                            // If s_rename_error_msg is set, stay in editing mode for user to fix.
                        }

                    } else {
                        if (ImGui::Selectable(s_profile_names[i].c_str(), is_selected_this_item, ImGuiSelectableFlags_AllowDoubleClick)) {
                            s_selected_profile_idx = i;
                            if (ImGui::IsMouseDoubleClicked(0)) { // Check for double click on this item
                                s_editing_profile_idx = i;
                                strncpy(s_edit_buffer, s_profile_names[i].c_str(), sizeof(s_edit_buffer) -1);
                                s_edit_buffer[sizeof(s_edit_buffer) - 1] = '\0';
                                s_last_click_time = -1.0f; // Reset double-click state
                                s_rename_error_msg = ""; // Clear rename error when starting new edit
                            } else { // Single click
                                // Handled by s_selected_profile_idx = i;
                                s_last_clicked_item_idx = i; // For your original double click logic if needed
                                s_last_click_time = ImGui::GetTime();
                            }
                        }
                         if (is_selected_this_item) ImGui::SetItemDefaultFocus();
                    }

					if (is_currently_loaded) {
                        ImGui::PopStyleColor();
                    }

					if (s_profile_names[i] == "(default)") {
						ImGui::PopStyleColor();
					}
                }
            }
            ImGui::EndChild(); // ##ProfilesOptionsList

            if (!s_rename_error_msg.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                ImGui::TextWrapped("%s", s_rename_error_msg.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::End(); // ##ProfilesDropUpMenu
            ImGui::PopStyleVar();
        }
    }
}

// Define sections

struct Section {
    std::string title;
    std::string description;
    bool optionA; 
    float settingValue; 
};

struct SectionConfig {
    const char *title;
    const char *description;
};

std::vector<Section> sections;

// Title + Description of Sections

static std::array<SectionConfig, section_amounts> SECTION_CONFIGS = {{
    {"Freeze", "Automatically Tab Glitch With a Button"},
    {"Item Desync", "Enable Item Collision (Hold Item Before Pressing)"},
    {"Helicopter High Jump", "Use COM Offset to Catapult Yourself Into The Air by Aligning your Back Angled to the Wall and Jumping and Letting Your Character Turn"},
    {"Speedglitch", "Use COM offset to Massively Increase Midair Speed"},
    {"Item Unequip COM Offset", "Automatically Do a /e dance2 Item COM Offset Where You Unequip the Item"},
    {"Press a Button", "Whenever You Press Your Keybind, it Presses the Other Button for One Frame"},
    {"Wallhop/Rotation", "Automatically Flick and Jump to easily Wallhop On All FPS"},
    {"Walless LHJ", "Lag High Jump Without a Wall by Offsetting COM Downwards or to the Right"},
    {"Item Clip", "Clip through 2-3 Stud Walls Using Gears"},
    {"Laugh Clip", "Automatically Perform a Laugh Clip"},
    {"Wall-Walk", "Walk Across Wall Seams Without Jumping"},
    {"Spam a Key", "Whenever You Press Your Keybind, it Spams the Other Button"},
    {"Ledge Bounce", "Briefly Falls off a Ledge to Then Bounce Off it While Falling"},
	{"Smart Bunnyhop", "Intelligently enables or disables Bunnyhop for any Key"}
}};

static void InitializeSections()
{
    sections.clear();
    if (shortdescriptions) {
        for (size_t i = 0; i < SECTION_CONFIGS.size(); ++i) {
			sections.push_back({
				SECTION_CONFIGS[i].title, "",
				false, // Fallback Option1
				50.0f  // Fallback Option2
			});
		}
	} else {
		for (size_t i = 0; i < SECTION_CONFIGS.size(); ++i) {
			sections.push_back({
				SECTION_CONFIGS[i].title, SECTION_CONFIGS[i].description,
				false, // Fallback Option1
				50.0f  // Fallback Option2
			});
		}
	}
}

struct BindingState {
    bool bindingMode = false;
    bool notBinding = true;
    std::chrono::steady_clock::time_point rebindTime;
    char keyBuffer[32] = "0x00";
    char keyBufferHuman[32] = "None";  // Make sure this exists!
    std::string buttonText = "Click to Bind Key";
    bool keyWasPressed[258] = {false};
    bool firstRun = true;
    int lastSelectedSection = -1;
};

// Global map to store binding states for each key variable
static std::unordered_map<unsigned int *, BindingState> g_bindingStates;

static unsigned int BindKeyMode(unsigned int* keyVar, unsigned int currentkey, int selected_section)
{
    // Get or create the state for this specific key variable
    BindingState& state = g_bindingStates[keyVar];
    if (state.bindingMode) {
	    notbinding = false;
        state.rebindTime = std::chrono::steady_clock::now();
        // Initialize key states on first run in binding mode
        if (state.firstRun) {
            for (int key = 1; key < 258; key++) {
                state.keyWasPressed[key] = IsKeyPressed(key);
            }
            state.firstRun = false;
        }
		// Check side-specific shifts first to avoid ambiguity
		static const int priorityKeys[] = {0xA1, 0xA0, 0x10}; // VK_RSHIFT, VK_LSHIFT, VK_SHIFT
		for (int priorityKey : priorityKeys) {
			bool currentlyPressed = IsKeyPressed(priorityKey);
			if (currentlyPressed && !state.keyWasPressed[priorityKey]) {
				state.bindingMode = false;
				state.firstRun = true;
				state.keyWasPressed[priorityKey] = true;
				std::snprintf(state.keyBuffer, sizeof(state.keyBuffer), "0x%02x", priorityKey);
				return priorityKey;
			}
			state.keyWasPressed[priorityKey] = currentlyPressed;
		}

		// Check all other keys
		for (int key = 1; key < 258; key++) {
			// Skip the priority keys we already checked
			if (key == 0xA1 || key == 0xA0 || key == 0x10) continue;
    
			bool currentlyPressed = IsKeyPressed(key);
			// Key was previously pressed and is now released - reset it
			if (state.keyWasPressed[key] && !currentlyPressed) {
				state.keyWasPressed[key] = false;
			}
			// Key is pressed now AND it wasn't pressed before (fresh press)
			if (currentlyPressed && !state.keyWasPressed[key]) {
				state.bindingMode = false;
				state.firstRun = true;
				state.keyWasPressed[key] = true;
        
				std::snprintf(state.keyBuffer, sizeof(state.keyBuffer), "0x%02x", key);
				return key;
			}
			// Update the previous state
			state.keyWasPressed[key] = currentlyPressed;
		}
    } else {
        // Reset firstRun when not in binding mode
        state.firstRun = true;
        // Check if the selected_section has changed
        if (selected_section != state.lastSelectedSection) {
            std::snprintf(state.keyBuffer, sizeof(state.keyBuffer), "0x%02x", currentkey);
            state.lastSelectedSection = selected_section;
        }
		// Fix for settings menu being broken if no other menu is open
		if (selected_section == -1) {
			std::snprintf(state.keyBuffer, sizeof(state.keyBuffer), "0x%02x", currentkey);
		}
        state.buttonText = "Click to Bind Key";
        auto currentTime = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsedtime = currentTime - state.rebindTime;

		// Wait .3 seconds after binding before allowing macro to trigger
        if (elapsedtime.count() >= 0.3) {
            state.notBinding = true; // I honestly forgot the point of this line
			notbinding = true;
        }
    }
    
    return currentkey;
}

static void GetKeyNameFromHex(unsigned int hexKeyCode, char* buffer, size_t bufferSize)
{
    // Clear the buffer
    memset(buffer, 0, bufferSize);

    // Map the virtual key code to a scan code
    UINT scanCode = MapVirtualKey(hexKeyCode, MAPVK_VK_TO_VSC);

    // Check if THIS specific key is currently being bound
    bool thisKeyIsBinding = false;
    for (auto &[keyPtr, state] : g_bindingStates) {
	    if (state.bindingMode && *keyPtr == hexKeyCode) {
		    thisKeyIsBinding = true;
		    break;
	    }
    }

    if (thisKeyIsBinding) { // Only disable for the key being bound
	    return;
    }

    // Attempt to get the readable key name
    if (GetKeyNameTextA(scanCode << 16, buffer, bufferSize) > 0) {
        // Successfully retrieved the key name
        return;
    } else {
        // If GetKeyNameText fails, try to find the VK_ name
        auto it = vkToString.find(hexKeyCode);
        if (it != vkToString.end()) {
            strncpy(buffer, it->second.c_str(), bufferSize - 1);
            buffer[bufferSize - 1] = '\0'; // Ensure null termination
        } else {
            // If not found, return a default hex representation
            snprintf(buffer, bufferSize - 1, "0x%X", hexKeyCode);
            buffer[bufferSize - 1] = '\0';
        }
    }
}

// Make the Title Bar Black
static bool SetTitleBarColor(HWND hwnd, COLORREF color) {
    BOOL value = TRUE;
    HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
    if (FAILED(hr)) {
        return false;
    }
    hr = DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &color, sizeof(color));
    if (FAILED(hr)) {
        return false;
    }
    return true;
}

// Disable windows auto-optimizations
static void DisablePowerThrottling() {
    PROCESS_POWER_THROTTLING_STATE state = {};
    state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED |
                        PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
    state.StateMask = 0;

    SetProcessInformation(GetCurrentProcess(),
                          ProcessPowerThrottling,
                          &state,
                          sizeof(state));
}

void CheckDisplayScale(HWND hwnd, int display_scale) {
    // Per-Monitor DPI Awareness
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    UINT dpi = GetDpiForWindow(hwnd);
    int currentScalePercent = (int)(dpi * 100 / USER_DEFAULT_SCREEN_DPI); // 96 DPI base

    if (currentScalePercent != 100 && display_scale != currentScalePercent) {
        std::wstring msg = L"Your display scaling doesn't match the program's settings. "
                           L"Your current display scale is " + std::to_wstring(currentScalePercent) +
                           L"%, the Macro's display scale is " + std::to_wstring(display_scale) +
                           L"%. Make these two equal by either updating the Macro's Settings or your Windows Settings.";

        MessageBox(hwnd,
                   msg.c_str(),
                   L"Display Scaling Mismatch",
                   MB_OK | MB_ICONWARNING);
    }
}

static void SetWorkingDirectoryToExecutablePath() // Allows non-standard execution for save files
{
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH)) {
	// Remove the executable name to get the directory
	char *lastSlash = strrchr(exePath, '\\');
	if (lastSlash) {
	    *lastSlash = '\0'; // Terminate the string at the last backslash
	    SetCurrentDirectoryA(exePath);
		}
    }
}

// START OF PROGRAM
static void RunGUI()
{
	// Set working directory to correct path
    SetWorkingDirectoryToExecutablePath();

	// Setup Linux Compatibility Layer if Needed
	InitLinuxCompatLayer();

	// Gui Thread has lower priority
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);

	// Initialize a basic Win32 window
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("Roblox Macro Client"), NULL };

	// Load icons
	wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wc.hIconSm = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));

	// Override old default profile to new one
	SaveSettings(G_SETTINGS_FILEPATH, "SAVE_DEFAULT_90493");
	
	TryLoadLastActiveProfile(G_SETTINGS_FILEPATH);

	// Load Settings
	LoadSettings(G_SETTINGS_FILEPATH, "SAVE_DEFAULT_90493"); // Only check for existence of default

	RegisterClassEx(&wc);
	HWND hwnd = CreateWindow(wc.lpszClassName, _T("Spencer Macro Client"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);
	::hwnd = hwnd;
	g_mouseWheel.Initialize(hwnd);
	SetTitleBarColor(hwnd, RGB(0, 0, 0));

	// Load Window coordinates and remove invalid coordinates

	if (WindowPosX < 0 || WindowPosX > 15360) {
		WindowPosX = 0;
	}

	if (WindowPosY < 0 || WindowPosY > 8640) {
		WindowPosY = 0;
	}

	if (screen_width < 0 || screen_width > 15360) {
		screen_width = 0;
	}

	if (screen_height < 0 || screen_height > 8640) {
		screen_height = 0;
	}

	if (WindowPosX == 0 && WindowPosY == 0) {
		SetWindowPos(hwnd, NULL, 0, 0, screen_width, screen_height, SWP_NOZORDER | SWP_NOMOVE);
	} else {
		SetWindowPos(hwnd, NULL, WindowPosX, WindowPosY, screen_width, screen_height, SWP_NOZORDER);
	}

	// Get raw screen rect position
	RECT raw_screen_rect;
	GetWindowRect(hwnd, &raw_screen_rect);
	raw_window_width = raw_screen_rect.right - raw_screen_rect.left;
	raw_window_height = raw_screen_rect.bottom - raw_screen_rect.top;

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd)) {
		CleanupDeviceD3D();
		UnregisterClass(wc.lpszClassName, wc.hInstance);
    }

	// Show the window
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, style | WS_EX_LAYERED);

    float alphaFraction = windowOpacityPercent / 100.0f;
    BYTE alphaByte = static_cast<BYTE>(alphaFraction * 255.0f);
    SetLayeredWindowAttributes(hwnd, 0, alphaByte, LWA_ALPHA); // Set to user opacity

	if (ontoptoggle) {
		SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = NULL;
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls

	HRSRC hRes = FindResource(NULL, TEXT("LSANS_TTF"), RT_RCDATA);
    HGLOBAL hMem = LoadResource(NULL, hRes);
	LPVOID pData = LockResource(hMem);
    DWORD size = SizeofResource(NULL, hRes);

	ImFont *mainfont = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(pData, size, 20.0f);

    // Initialize ImGui for Win32 and DirectX 11
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

	auto lastTime = std::chrono::steady_clock::now();
	constexpr float targetFrameTime = 1.0f / 90.0f;  // 90 FPS target
	auto nextFrameTime = lastTime + std::chrono::duration<float>(targetFrameTime);

	InitializeSections();

	MSG msg;

	// Update Specific Variables on startup (Update int values from the corresponding char values)

	#define SAFE_CONVERT_INT(var, src) \
		try { var = std::stoi(src); } catch (...) {}

	#define SAFE_CONVERT_FLOAT(var, src) \
		try { var = std::stof(src); } catch (...) {}

	#define SAFE_CONVERT_DOUBLE(var, src) \
		try { var = std::stod(src); } catch (...) {}

	SAFE_CONVERT_INT(WallhopDelay, WallhopDelayChar);
	SAFE_CONVERT_INT(WallhopBonusDelay, WallhopBonusDelayChar);
	SAFE_CONVERT_INT(clip_slot, ItemClipSlot);
	SAFE_CONVERT_INT(desync_slot, ItemDesyncSlot);
	SAFE_CONVERT_INT(clip_delay, ItemClipDelay);
	SAFE_CONVERT_INT(RobloxFPS, RobloxFPSChar);
	SAFE_CONVERT_INT(AntiAFKTime, AntiAFKTimeChar);
	SAFE_CONVERT_INT(PressKeyDelay, PressKeyDelayChar);
	SAFE_CONVERT_INT(PressKeyBonusDelay, PressKeyBonusDelayChar);
	SAFE_CONVERT_INT(PasteDelay, PasteDelayChar);
	SAFE_CONVERT_INT(HHJLength, HHJLengthChar);
	SAFE_CONVERT_INT(HHJFreezeDelayOverride, HHJFreezeDelayOverrideChar);
	SAFE_CONVERT_INT(HHJDelay1, HHJDelay1Char);
	SAFE_CONVERT_INT(HHJDelay2, HHJDelay2Char);
	SAFE_CONVERT_INT(HHJDelay3, HHJDelay3Char);

	SAFE_CONVERT_DOUBLE(BunnyHopDelay, BunnyHopDelayChar);

	// Special Cases
	try {
		speed_strengthx = std::stoi(RobloxPixelValueChar);
		speed_strengthy = -speed_strengthx;
	} catch (...) {}

	try {
		spam_delay = std::stof(SpamDelay);
		real_delay = static_cast<int>((spam_delay + 0.5f) / 2);
	} catch (...) {}

	// Overwrite default RobloxPlayerBeta to sober if we're on linux
	if (strcmp(settingsBuffer, "RobloxPlayerBeta.exe") == 0 && g_isLinuxWine) {
		std::snprintf(settingsBuffer, sizeof(settingsBuffer), "sober");
	}

	// Change pasting delay to be more consistent if we're on linux
	if (strcmp(PasteDelayChar, "1") == 0 && g_isLinuxWine) {
		std::snprintf(PasteDelayChar, sizeof(PasteDelayChar), "35");
		PasteDelay = 35;
		chatoverride = false;
	}

	// Modify HHJ timings to switch to linux if we see regular timings active
	if (HHJDelay1 == 9 && HHJDelay2 == 17 && HHJDelay3 == 16 && g_isLinuxWine) {
		HHJDelay1 = 0;
		std::snprintf(HHJDelay1Char, sizeof(HHJDelay1Char), "0");
		HHJDelay2 = 0;
		std::snprintf(HHJDelay2Char, sizeof(HHJDelay2Char), "0");
		HHJDelay3 = 17;
		std::snprintf(HHJDelay3Char, sizeof(HHJDelay3Char), "17");
	}

    // Attach the GUI thread to the input of the main thread
    DWORD mainThreadId = GetWindowThreadProcessId(hwnd, NULL);
    DWORD guiThreadId = GetCurrentThreadId();
    AttachThreadInput(mainThreadId, guiThreadId, TRUE); // Attach the threads

    // Set window flags to disable resizing, moving, and title bar
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize |
                                        ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoTitleBar |
										ImGuiWindowFlags_NoScrollbar |
										ImGuiWindowFlags_NoScrollWithMouse |
										ImGuiWindowFlags_NoBringToFrontOnFocus;

	// Check if the display scale is accurate to the macro
	CheckDisplayScale(hwnd, display_scale);

	bool amIFocused = true;
	bool processFoundOld = false;
	bool renderfirstframe = true;

	while (running) {
		// Process all pending messages first
		while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				running = false;
				break;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (!running) break;

		// RENDER ONLY IF YOU'RE FOCUSED OR PROCESSFOUND CHANGES
		if (hwnd) { 
			amIFocused = (GetForegroundWindow() == hwnd);
		}

		if (processFoundOld != processFound) {
			amIFocused = true;
		}

		processFoundOld = processFound;

		if (renderfirstframe) {
			amIFocused = true;
			renderfirstframe = false;
		}

		if (!amIFocused && !g_isLinuxWine) {
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			continue; // Skip the rest of this iteration
		}


		// Check if it's time to render
		auto currentTime = std::chrono::steady_clock::now();
		if (currentTime >= nextFrameTime) {

			// Update frame timing
			lastTime = currentTime;
			nextFrameTime = currentTime + std::chrono::duration<float>(targetFrameTime);

			// Start ImGui frame
			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

            // ImGui window dimensions
            ImVec2 display_size = ImGui::GetIO().DisplaySize;

            // Set the size of the main ImGui window to fill the screen, fitting to the top left
            ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);

            // Create the main window with the specified flags
            ImGui::Begin("Main SMU Window", nullptr, window_flags); // Main ImGui window

			// Enable ImGUI Debug Mode
			// ImGui::ShowMetricsWindow();

            // Top settings child window
            float settings_panel_height = 140;
            ImGui::BeginChild("GlobalSettings", ImVec2(display_size.x - 16, settings_panel_height), true);

            // Start of Global Settings
			ImGui::AlignTextToFramePadding();
            ImGui::TextWrapped("Global Settings");
			if (UserOutdated) {
				ImGui::SameLine(135);
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
				ImGui::TextWrapped("(OUTDATED VERSION)");
				ImGui::PopStyleColor();
			}

			ImGui::SameLine(ImGui::GetWindowWidth() - 795);
			ImGui::TextWrapped("DISCLAIMER: THIS IS NOT A CHEAT, IT NEVER INTERACTS WITH ROBLOX MEMORY.");

			// Macro Toggle Checkbox
			ImGui::PushStyleColor(ImGuiCol_Text, macrotoggled ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
			ImGui::AlignTextToFramePadding();
            ImGui::Checkbox("Macro Toggle (Anti-AFK remains!)", &macrotoggled); // Checkbox for toggling
			ImGui::PopStyleColor();

			ImGui::SameLine(ImGui::GetWindowWidth() - 790);
			ImGui::TextWrapped("The ONLY official source for this is");
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(50, 102, 205, 255)); // Blue
			ImGui::SameLine(ImGui::GetWindowWidth() - 499);
			ImGui::TextWrapped("https://github.com/Spencer0187/Spencer-Macro-Utilities");
			if (ImGui::IsItemHovered()) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				if (ImGui::IsItemClicked()) {
						ShellExecuteA(NULL, "open", "https://github.com/Spencer0187/Spencer-Macro-Utilities", NULL, NULL, SW_SHOWNORMAL);
				}
			}

			ImGui::PopStyleColor();
			ImGui::AlignTextToFramePadding();
			ImGui::TextWrapped(g_isLinuxWine ? "Roblox Executable Name/PIDs (Space Separated)" : "Roblox Executable Name");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(250.0f);

			ImGui::InputText("##SettingsTextbox", settingsBuffer, sizeof(settingsBuffer), g_isLinuxWine ? 0 : ImGuiInputTextFlags_CharsNoBlank); // Textbox for input, remove blank characters

			ImGui::SameLine();

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 pos = ImGui::GetCursorScreenPos();
			pos.y += ImGui::GetTextLineHeight() / 2 - 3;
			ImU32 color = processFound ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255);

			drawList->AddCircleFilled(ImVec2(pos.x + 5, pos.y + 5), 5, color);
			ImGui::Dummy(ImVec2(5 * 2, 5 * 2));

			if (!processFound) {
				ImGui::SameLine();
				if (!g_isLinuxWine){ImGui::TextWrapped("Roblox Not Found");}
			}

			ImGui::SameLine(ImGui::GetWindowWidth() - 360);
			ImVec2 tooltipcursorpos = ImGui::GetCursorScreenPos();
			ImGui::Text("Toggle Anti-AFK ("); ImGui::SameLine(0, 0);
			ImGui::TextColored(ImColor(50, 102, 205, 255), "?"); ImGui::SameLine(0, 0);
			ImGui::Text("):");
			ImGui::SetCursorScreenPos(tooltipcursorpos);
			ImVec2 TextSizeCalc = ImGui::CalcTextSize("Toggle Anti-AFK (?)");
			ImGui::InvisibleButton("##tooltip", TextSizeCalc);
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("Anti-AFK only works on Windows right now due to Linux restraints.\nAnti-AFK functions by counting up a timer constantly. If you are tabbed into roblox\n"
									"and you press any key on your keyboard, the timer resets.\nIf the timer expires, if the current active window is Roblox, it will press the \"\\\" key\n"
									"two times, which will toggle on and off UI navigation.\n\nIf you are outside of Roblox, it will manually tab into Roblox, and then perform the action.\n"
									"You may change the key anti-afk presses. If you find a less instrusive\nkey that still works against Roblox's AFK timeout, please join and tell the discord.");

			ImGui::SetCursorScreenPos(ImVec2(tooltipcursorpos.x + TextSizeCalc.x, tooltipcursorpos.y));
			ImGui::SameLine(ImGui::GetCursorScreenPos().x + 5);
			ImGui::Checkbox("##AntiAFKToggle", &antiafktoggle);

			ImGui::SameLine(ImGui::GetWindowWidth() - 150);
			ImGui::Text("VERSION 3.1.0");

			ImGui::AlignTextToFramePadding();
			ImGui::TextWrapped("Roblox Sensitivity (0-4):");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(70.0f);
			if (ImGui::InputText("##Sens", RobloxSensValue, sizeof(RobloxSensValue), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
				PreviousSensValue = -1;
				// Update Wallhop Degrees every change only if sensitivity is not zero/null
				float sensValue = std::atof(RobloxSensValue);
				if (sensValue != 0.0f) {
					float pixels = std::atof(WallhopDegrees) * (camfixtoggle ? 1000.0f : 720.0f) / (360.0f * sensValue);
					snprintf(WallhopPixels, sizeof(WallhopPixels), "%.0f", pixels);
					try {
						wallhop_dx = std::round(std::stoi(WallhopPixels));
						wallhop_dy = -std::round(std::stoi(WallhopPixels));
					} catch (const std::invalid_argument &e) {
					} catch (const std::out_of_range &e) {
					}
				}
			}
			ImGui::SameLine();
			ImGui::TextWrapped("Your Roblox FPS:");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(40.0f);
			if (ImGui::InputText("##FPS", RobloxFPSChar, sizeof(RobloxFPSChar), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
				try {
					RobloxFPS = std::stoi(RobloxFPSChar);
				} catch (const std::invalid_argument &e) {
				} catch (const std::out_of_range &e) {
				}
			}
			ImGui::SameLine();
			ImGui::Text("Game Uses Cam-Fix:");
			ImGui::SameLine();
			
			if (ImGui::Checkbox("##CamfixToggle", &camfixtoggle) || PreviousSensValue == -1) {
				wallhopupdate = false;
				if (PreviousSensValue != -1) { // Localize wallhop updates only to cam-fix
					wallhopupdate = true;
				}
				PreviousSensValue = -1;

				PreviousWallWalkValue = -1;
				try {
					if (wallhopupdate) {
						float factor = camfixtoggle ? 1.388888889f : 1.0f / 1.388888889f;
						if (wallhopswitch) {
							wallhop_dx = std::round(std::stoi(WallhopPixels) * (camfixtoggle ? -factor : factor));
							wallhop_dy = std::round(std::stoi(WallhopPixels) * (camfixtoggle ? factor : -factor));
						} else {
							wallhop_dx = std::round(std::stoi(WallhopPixels) * factor);
							wallhop_dy = std::round(std::stoi(WallhopPixels) * -factor);
							sprintf(WallhopPixels, "%d", wallhop_dx);
						}
					}
				} catch (const std::invalid_argument &) {
				} catch (const std::out_of_range &) {
				}

				float CurrentWallWalkValue = atof(RobloxSensValue); // Wallwalk

				float baseValue = camfixtoggle ? 500.0f : 360.0f;
				wallwalk_strengthx = -static_cast<int>(std::round((baseValue / CurrentWallWalkValue) * 0.13f));
				wallwalk_strengthy = static_cast<int>(std::round((baseValue / CurrentWallWalkValue) * 0.13f));

				sprintf(RobloxWallWalkValueChar, "%d", wallwalk_strengthx);

				float CurrentSensValue = atof(RobloxSensValue); // Speedglitch

				try {
					float baseValue = camfixtoggle ? 500.0f : 360.0f;
					float multiplier = (359.0f / 360.0f) * (359.0f / 360.0f);
					RobloxPixelValue = static_cast<int>(std::round((baseValue / CurrentSensValue) * multiplier));
				} catch (const std::invalid_argument &) {
				} catch (const std::out_of_range &) {
				}

				PreviousSensValue = CurrentSensValue;
				sprintf(RobloxPixelValueChar, "%d", RobloxPixelValue);
				try { // Error Handling
					speed_strengthx = std::stoi(RobloxPixelValueChar);
					speed_strengthy = -std::stoi(RobloxPixelValueChar);
				} catch (const std::invalid_argument &e) {
				} catch (const std::out_of_range &e) {
				}

			}

			static bool show_settings_menu = false;

			ImGui::SameLine(ImGui::GetWindowWidth() - 100);
			if (show_settings_menu ? ImGui::Button("Settings <") : ImGui::Button("Settings >")) {
				show_settings_menu = !show_settings_menu;
			}

			if (show_settings_menu) {
				// Get the main window size
				ImVec2 main_window_size = ImGui::GetIO().DisplaySize;
				float child_width = main_window_size.x * 0.5f;
				float child_height = main_window_size.y * 0.5f;

				// Calculate position to center the child window
				ImVec2 child_pos = ImVec2(
					(main_window_size.x * 0.4f),
					(main_window_size.y - child_height - 90) * 0.5f
				);

				// Set the next window's position and size
				ImGui::SetNextWindowPos(child_pos, ImGuiCond_Once);
				ImGui::SetNextWindowSize(ImVec2(child_width, child_height), ImGuiCond_Always);

				// Begin the child window (non-draggable)
				ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

				if (ImGui::Begin("Settings Menu", &show_settings_menu, window_flags)) {
					// Begin a scrollable child region for the settings list
					ImGui::BeginChild("SettingsList", ImVec2(0, 0), true);

					// Setting: Windows Display Scale
					ImGui::Text("Your Current Windows Display Scale Value (10-500%):");
					ImGui::SetNextItemWidth(150);

					if (ImGui::InputInt("##DisplayScale", &display_scale)) {
						if (display_scale < 10)
							display_scale = 10;
						if (display_scale > 500)
							display_scale = 500;
					}
					ImGui::SameLine();
					ImGui::Text("%%");

					ImGui::Separator();

					BindingState& shiftState = g_bindingStates[&vk_shiftkey];
    
					ImGui::TextWrapped("Custom Shiftlock Key:");
    
					if (ImGui::Button((shiftState.buttonText + "##ShiftKey").c_str())) {
						shiftState.bindingMode = true;
						shiftState.notBinding = false;
						shiftState.buttonText = "Press a Key...";
					}
    
					ImGui::SameLine();
					vk_shiftkey = BindKeyMode(&vk_shiftkey, vk_shiftkey, selected_section);
    
					// Human readable display
					ImGui::SetNextItemWidth(150.0f);
					GetKeyNameFromHex(vk_shiftkey, shiftState.keyBufferHuman, sizeof(shiftState.keyBufferHuman));
					ImGui::InputText("##ShiftKeyHuman", shiftState.keyBufferHuman, sizeof(shiftState.keyBufferHuman), ImGuiInputTextFlags_ReadOnly);
					ImGui::SameLine();
					ImGui::TextWrapped("Key Binding");
    
					// Hexadecimal display
					ImGui::SetNextItemWidth(50.0f);
					ImGui::PushID("ShiftKeyHex");
					ImGui::SameLine();
					ImGui::InputText("##ShiftKeyHex", shiftState.keyBuffer, sizeof(shiftState.keyBuffer), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_ReadOnly);
					ImGui::PopID();
					ImGui::SameLine();
					ImGui::TextWrapped("Hex");

					ImGui::Separator();

					ImGui::Checkbox("Force-Set Chat Open Key to \"/\" (Most Stable)", &chatoverride);

					ImGui::Separator();

					BindingState& chatState = g_bindingStates[&vk_chatkey];

					ImGui::TextWrapped("Custom Chat Key (Must disable Force-Set):");

					// Bind button
					if (ImGui::Button((chatState.buttonText + "##ChatKey").c_str())) {
						chatState.bindingMode = true;
						chatState.notBinding = false;
						chatState.buttonText = "Press a Key...";
					}

					ImGui::SameLine();
					vk_chatkey = BindKeyMode(&vk_chatkey, vk_chatkey, selected_section);

					// Human readable display
					ImGui::SetNextItemWidth(150.0f);
					GetKeyNameFromHex(vk_chatkey, chatState.keyBufferHuman, sizeof(chatState.keyBufferHuman));
					ImGui::InputText("##ChatKeyHuman", chatState.keyBufferHuman, sizeof(chatState.keyBufferHuman), ImGuiInputTextFlags_ReadOnly);
					ImGui::SameLine();
					ImGui::TextWrapped("Key Binding");

					// Hexadecimal display
					ImGui::SameLine();
					ImGui::SetNextItemWidth(50.0f);
					ImGui::PushID("ChatKeyHex");
					ImGui::InputText("##ChatKeyHex", chatState.keyBuffer, sizeof(chatState.keyBuffer), 
									 ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_ReadOnly);
					ImGui::PopID();
					ImGui::SameLine();
					ImGui::TextWrapped("Hex");

					ImGui::Separator();

					ImGui::Checkbox("##Oldpaste", &useoldpaste);
					ImGui::SameLine();
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 4);
					ImGui::TextWrapped("Old Unicode Chat-Typing (Works across languages but may be blocked by some anticheats)");

					ImGui::Separator();

					ImGui::AlignTextToFramePadding();
					ImGui::TextWrapped("Delay between every key press in chat (ms):");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(30.0f);
					if (ImGui::InputText("##PasteDelay", PasteDelayChar, sizeof(PasteDelayChar), ImGuiInputTextFlags_CharsDecimal |ImGuiInputTextFlags_CharsNoBlank)) {
						try {
							PasteDelay = std::stoi(PasteDelayChar);
						} catch (const std::invalid_argument &e) {
						} catch (const std::out_of_range &e) {
						}
					}

					ImGui::Separator();

					BindingState& afkState = g_bindingStates[&vk_afkkey];

					ImGui::TextWrapped("Custom Anti-AFK Key (That the macro uses):");

					// Bind button
					if (ImGui::Button((afkState.buttonText + "##Afkkey").c_str())) {
						afkState.bindingMode = true;
						afkState.notBinding = false;
						afkState.buttonText = "Press a Key...";
					}

					ImGui::SameLine();
					vk_afkkey = BindKeyMode(&vk_afkkey, vk_afkkey, selected_section);

					// Human readable display
					ImGui::SetNextItemWidth(150.0f);
					GetKeyNameFromHex(vk_afkkey, afkState.keyBufferHuman, sizeof(afkState.keyBufferHuman));
					ImGui::InputText("##AFKKeyHuman", afkState.keyBufferHuman, sizeof(afkState.keyBufferHuman), ImGuiInputTextFlags_ReadOnly);
					ImGui::SameLine();
					ImGui::TextWrapped("Key Binding");

					// Hexadecimal display
					ImGui::SameLine();
					ImGui::SetNextItemWidth(50.0f);
					ImGui::PushID("AfkkeyHex");
					ImGui::InputText("##AfkkeyHex", afkState.keyBuffer, sizeof(afkState.keyBuffer), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_ReadOnly);
					ImGui::PopID();
					ImGui::SameLine();
					ImGui::TextWrapped("Hex");

					ImGui::Separator();

					ImGui::AlignTextToFramePadding();
					ImGui::Text("Amount of Minutes Between Anti-AFK Runs:");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(30.0f);
					if (ImGui::InputText("##AntiAFKTime", AntiAFKTimeChar, sizeof(AntiAFKTimeChar), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
						try {
							AntiAFKTime = std::stoi(AntiAFKTimeChar);
						} catch (const std::invalid_argument &e) {
						} catch (const std::out_of_range &e) {
						}
					}

					ImGui::Separator();

					ImGui::Checkbox("Double-Press AFK keybind during Anti-AFK", &doublepressafkkey);

					ImGui::Separator();
					
					if (ImGui::Checkbox("Remove Side-Bar Macro Descriptions", &shortdescriptions)) {
						InitializeSections();
					}

					ImGui::Separator();

					ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight() * 0.5f));

					ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(50, 102, 205, 255)); // Blue
					ImGui::Text("%s", "Want to Donate directly to my Github?");
					if (ImGui::IsItemHovered()) {
						ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
						if (ImGui::IsItemClicked()) {
							ShellExecuteA(NULL, "open", "https://github.com/sponsors/Spencer0187", NULL, NULL, SW_SHOWNORMAL);
						}
					}
					ImGui::PopStyleColor();

					ImGui::Text("%s", "Github Doesn't take any of the profits.");

					// End the scrollable child region
					ImGui::EndChild();
				}
				ImGui::End();
			}

            ImGui::EndChild(); // End Global Settings child window

			// Calculate left panel width and height
			float left_panel_width = ImGui::GetWindowSize().x * 0.3f - 23;

			ImGui::BeginChild("LeftScrollSection", ImVec2(left_panel_width, ImGui::GetWindowSize().y - settings_panel_height - 20), true);

			for (size_t display_index = 0; display_index < section_amounts; ++display_index) {

				int i = section_order[display_index]; // Get section index from order array

				ImGui::PushID(i);

				float buttonWidth = left_panel_width - ImGui::GetStyle().FramePadding.x * 2;

				// Set up button colors based on toggle state
				if (section_toggles[i]) {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.29f, 0.45f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.06f, 0.53f, 0.98f, 1.0f));
				} else {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.29f, 0.15f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.98f, 0.59f, 0.26f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.98f, 0.53f, 0.06f, 1.0f));
				}

				// Calculate button height based on text
				ImVec2 titleSize = ImGui::CalcTextSize(sections[i].title.c_str(), nullptr, true);
				ImVec2 descriptionSize = ImGui::CalcTextSize(sections[i].description.c_str(), nullptr, true, buttonWidth - 20);
				float buttonHeight = titleSize.y + descriptionSize.y + ImGui::GetStyle().FramePadding.y * 2;


				// Create the button with a custom layout
				if (ImGui::GetScrollMaxY() == 0) {
					if (ImGui::Button("", ImVec2(buttonWidth - 7, buttonHeight))) {
						selected_section = i;
					}
				} else {
					if (ImGui::Button("", ImVec2(buttonWidth - 18, buttonHeight))) {
						selected_section = i;
					}
				}


				// Drag and Drop Source
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
					ImGui::SetDragDropPayload("DND_SECTION", &display_index, sizeof(int)); // Dragging by visual index
					ImGui::EndDragDropSource();
				}

				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_SECTION")) {
						int payload_index = *(const int *)payload->Data;
						std::swap(section_order[payload_index], section_order[display_index]);
					}
					ImGui::EndDragDropTarget();
				}

				// Custom text rendering on buttons

				ImVec2 buttonPos = ImGui::GetItemRectMin();
				ImVec2 textPos = ImVec2(buttonPos.x + ImGui::GetStyle().FramePadding.x, buttonPos.y + ImGui::GetStyle().FramePadding.y);
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), sections[i].title.c_str());

				// Wrap and draw description
				std::stringstream ss(sections[i].description);
				std::string word, currentLine;
				textPos.y += titleSize.y;
				while (ss >> word) {
					std::string potentialLine = currentLine + (currentLine.empty() ? "" : " ") + word;
					ImVec2 potentialLineSize = ImGui::CalcTextSize(potentialLine.c_str());

				if (ImGui::GetScrollMaxY() == 0) { // No scrollbar
					if (potentialLineSize.x > buttonWidth - 7) {
						// Draw the current line and move to the next
						drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), currentLine.c_str());
						textPos.y += potentialLineSize.y;
						currentLine = word;
					} else {
						currentLine = potentialLine;
					}
				} else {
					if (potentialLineSize.x > buttonWidth - 18) { // Scrollbar
						// Draw the current line and move to the next
						drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), currentLine.c_str());
						textPos.y += potentialLineSize.y;
						currentLine = word;
					} else {
						currentLine = potentialLine;
					}
				}
				}

				if (!currentLine.empty()) {
					drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), currentLine.c_str());
				}

				ImGui::PopStyleColor(3);
				ImGui::PopID();

				ImGui::Separator();
			}

			ImGui::EndChild();

            // Right section
            ImGui::SameLine(); // Move to the right of the left section

			ImVec2 rightSectionSize(
				display_size.x - 23 - left_panel_width,
				display_size.y - settings_panel_height - 20 - 30
			);

            // Right child window with dynamic sizing
			ImGui::BeginChild("RightSection", rightSectionSize, true);

			// Display different content based on the selected section
			if (selected_section >= 0 && selected_section < sections.size()) {
				// Get the binding state for this section's key
				unsigned int* currentKey = section_to_key.at(selected_section);
				BindingState& state = g_bindingStates[currentKey];
    
				// Display section title and keybind UI
				ImGui::TextWrapped("Settings for %s", sections[selected_section].title.c_str());
				ImGui::Separator();
				ImGui::NewLine();
				ImGui::TextWrapped("Keybind:");
				ImGui::SameLine();

				// Keybind button - use the state's buttonText
				if (ImGui::Button(state.buttonText.c_str())) {
					state.bindingMode = true;
					state.notBinding = false;
					state.buttonText = "Press a Key...";
				}

				ImGui::SameLine();

				// Handle key bindings for all sections
				if (section_to_key.count(selected_section)) {
					*currentKey = BindKeyMode(currentKey, *currentKey, selected_section);
					ImGui::SetNextItemWidth(150.0f);
        
					GetKeyNameFromHex(*currentKey, state.keyBufferHuman, sizeof(state.keyBufferHuman));
				}

				// Use the state's keyBuffer for display
				ImGui::InputText("##KeyBufferHuman", state.keyBufferHuman, sizeof(state.keyBufferHuman), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_ReadOnly);
				ImGui::SameLine();
				ImGui::TextWrapped("Key Binding");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(50.0f);

				ImGui::InputText("##KeyBuffer", state.keyBuffer, sizeof(state.keyBuffer), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_ReadOnly);

				ImGui::SameLine();
				ImGui::TextWrapped("Key Binding (Hexadecimal)");
				ImGui::PushStyleColor(ImGuiCol_Text, section_toggles[selected_section] ? 
										ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
				ImGui::TextWrapped("Enable This Macro:");
				ImGui::PopStyleColor();
				ImGui::SameLine();
				if (selected_section >= 0 && selected_section < section_amounts) {
					ImGui::Checkbox(("##SectionToggle" + std::to_string(selected_section)).c_str(), 
									&section_toggles[selected_section]);
				}
				ImGui::SameLine(243);
				ImGui::TextWrapped("(Human-Readable)");

				if (selected_section == 0) { // Freeze Macro
					ImGui::TextWrapped("Automatically Unfreeze for default 50ms after this amount of seconds (Anti-Internet-Kick)");
					ImGui::SetNextItemWidth(60.0f);
					ImGui::InputFloat("##FreezeFloat", &maxfreezetime, 0.0f, 0.0f, "%.2f");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(300.0f);
					ImGui::SliderFloat("##FreezeSlider", &maxfreezetime, 0.0f, 9.8f, "%.2f Seconds");

					char maxfreezeoverrideBuffer[16];
					std::snprintf(maxfreezeoverrideBuffer, sizeof(maxfreezeoverrideBuffer), "%d", maxfreezeoverride);

					ImGui::SetNextItemWidth(50.0f);
					if (ImGui::InputText("Modify 50ms Default Unfreeze Time (MS)", maxfreezeoverrideBuffer, sizeof(maxfreezeoverrideBuffer), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
						maxfreezeoverride = std::atoi(maxfreezeoverrideBuffer);
					}

					ImGui::Checkbox("Allow Roblox to be frozen while not tabbed in", &freezeoutsideroblox);
					ImGui::Checkbox("Switch from Hold Key to Toggle Key", &isfreezeswitch);
					if (isfreezeswitch || takeallprocessids) {
						freezeoutsideroblox = true;
					}

					ImGui::Checkbox("Freeze all Found Processes Instead of Newest", &takeallprocessids);

					ImGui::SameLine();
					ImGui::TextWrapped("(ONLY EVER USE FOR COMPATIBILITY ISSUES WITH NON-ROBLOX GAMES)");
					ImGui::Separator();
					ImGui::TextWrapped("Explanation:");
					ImGui::NewLine();
					ImGui::TextWrapped("Hold the hotkey to freeze your game, let go of it to release it. Suspending your game also pauses "
										"ALL network and physics activity that the server sends or recieves from you.");

				}

				if (selected_section == 1) { // Item Desync
					ImGui::TextWrapped("Gear Slot:");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(30.0f);
					ImGui::InputText("##ItemDesync", ItemDesyncSlot, sizeof(ItemDesyncSlot), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank);
					try {
						desync_slot = std::stoi(ItemDesyncSlot);
					} catch (const std::invalid_argument &e) {
					} catch (const std::out_of_range &e) {
					}
					ImGui::Separator();
					ImGui::TextWrapped("Equip your item inside of the slot you have picked here, then hold the keybind for 4-7 seconds");
					ImGui::Separator();
					ImGui::TextWrapped("Explanation:");
					ImGui::NewLine();
					ImGui::TextWrapped("This Macro rapidly sends number inputs to your roblox client, enough that the server begins to throttle "
										"you. The item that you're holding must not make a serverside sound, else desyncing yourself will be "
										"very buggy, and you will be unable to send any physics data to the server. Once you have desynced, "
										"the server will assume you're not holding an item, but your client will, which permanently enables "
										"client-side collision on the item.");
					ImGui::Separator();
					ImGui::TextWrapped(
										"Also, for convenience sake, you cannot activate desync unless you're tabbed into roblox, You will "
										"most likely crash any other program if you activate it in there.");
				}

				if (selected_section == 2) { // HHJ
					ImGui::Checkbox("Automatically time inputs", &autotoggle);
					ImGui::SameLine();
					ImGui::TextWrapped("(EXTREMELY BUGGY/EXPERIMENTAL, WORKS BEST ON HIGH FPS AND SHALLOW ANGLE TO WALL)");
					ImGui::Checkbox("Reduce Time Spent Frozen (For speedrunning only)", &fasthhj);
					ImGui::Checkbox("Replace Shiftlock with Zooming In", &hhjzoomin);
					ImGui::SameLine();
					ImGui::Checkbox("Reverse Direction?", &hhjzoominreverse);
					ImGui::TextWrapped("Length of HHJ flicks (ms):");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(52);
					if (ImGui::InputText("##HHJLength", HHJLengthChar, sizeof(HHJLengthChar), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
						try {
							HHJLength = std::stoi(HHJLengthChar);
						} catch (const std::invalid_argument &e) {
						} catch (const std::out_of_range &e) {
						}
					}

					ImGui::TextWrapped("(ADVANCED) Override current freeze delay (Non Zero) (ms): ");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(52);
					if (ImGui::InputText("##HHJFreezeDelayOverride", HHJFreezeDelayOverrideChar, sizeof(HHJFreezeDelayOverrideChar), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
						try {
							HHJFreezeDelayOverride = std::stoi(HHJFreezeDelayOverrideChar);
						} catch (const std::invalid_argument &e) {
						} catch (const std::out_of_range &e) {
						}
					}

					ImGui::TextWrapped("(ADVANCED) Delay after freezing before shiftlock is held (ms): ");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(52);
					if (ImGui::InputText("##HHJDelay1", HHJDelay1Char, sizeof(HHJDelay1Char), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
						try {
							HHJDelay1 = std::stoi(HHJDelay1Char);
						} catch (const std::invalid_argument &e) {
						} catch (const std::out_of_range &e) {
						}
					}

					ImGui::TextWrapped("(ADVANCED) Time shiftlock is held before spinning (ms): ");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(52);
					if (ImGui::InputText("##HHJDelay2", HHJDelay2Char, sizeof(HHJDelay2Char), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
						try {
							HHJDelay2 = std::stoi(HHJDelay2Char);
						} catch (const std::invalid_argument &e) {
						} catch (const std::out_of_range &e) {
						}
					}

					ImGui::TextWrapped("(ADVANCED) Time shiftlock is held after freezing (ms): ");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(52);
					if (ImGui::InputText("##HHJDelay3", HHJDelay3Char, sizeof(HHJDelay3Char), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
						try {
							HHJDelay3 = std::stoi(HHJDelay3Char);
						} catch (const std::invalid_argument &e) {
						} catch (const std::out_of_range &e) {
						}
					}

					ImVec2 tooltipcursorpos = ImGui::GetCursorScreenPos();
					ImGui::TextColored(ImColor(50, 102, 205, 255), "What is this ("); ImGui::SameLine(0, 0);
					ImGui::Text("?"); ImGui::SameLine(0, 0);
					ImGui::TextColored(ImColor(50, 102, 205, 255), ")");
					ImGui::SetCursorScreenPos(tooltipcursorpos);
					ImGui::InvisibleButton("##tooltip", ImGui::CalcTextSize("What is this (?)"));
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
						ImGui::SetTooltip("These are advanced options configurable for HHJ.\nThey modify the logic of what occurs after the unfreeze.\n"
										  "If you figure out a combination of these first two values that\nconsistently leads to higher HHJ's, please "
										  "tell the Roblox Glitching Community Discord.\n\nThe Defaults of each option are 9, 17, and 16 by default for Windows,\nand "
										  "0, 0, 17 on Linux Respectively.");
					ImGui::SetCursorScreenPos(ImVec2(tooltipcursorpos.x + ImGui::CalcTextSize("What is this (?)").x, tooltipcursorpos.y));
					ImGui::NewLine();

					ImGui::Separator();
					ImGui::TextWrapped("This module abuses Roblox's conversion from angular velocity to regular velocity, and its flawed centre of mass calculation.");
					ImGui::Separator();
					ImGui::TextWrapped("IMPORTANT:");
					ImGui::TextWrapped("Have your Sensitivity and Cam-Fix options set before using this module.");
					ImGui::Separator();
					ImGui::TextWrapped("Explanation:");
					ImGui::NewLine();
					ImGui::TextWrapped("Assuming unequip com offset set to /e dance2 is used prior to offset com, to perform a Helicopter High Jump, "
										"you want to align yourself with your back against the wall, and rotate slightly to the left (around 5-15 degrees). "
										"Now, turn your camera to face directly towards the wall, turn it towards the left a similar amount (5-15 degrees), "
										"in such a way that when you hold W, you turn INTO the wall, instead of away from it (the smaller the angle, the more "
										"successful you'll be). Now, still keeping the alignment and camera angle, perform a normal lag high jump without "
										"holding any movement keys. Instead of lagging, hold w, and press the assigned hotkey.");

					ImGui::Separator();
					ImGui::TextWrapped("If you are struggling with the lag high jump timing part, you can try using the \"Automatically time inputs\" feature. "
										"Align in the exact same way as stated above, but instead doing the lhj motion, just press the assigned key. This should time "
										"the two jumps, as well as the w tap for you. This can also act as a demonstration for what to do, when using manual activation of the module.");

				}

				if (selected_section == 3) { // Speedglitch

					float CurrentSensValue = atof(RobloxSensValue);
					if (CurrentSensValue != PreviousSensValue) {
						if (camfixtoggle) {
							try {
								RobloxPixelValue = static_cast<int>(((500.0f / CurrentSensValue) * (static_cast<float>(359) / 360)) + 0.5f);
							} catch (const std::invalid_argument &e) {
							} catch (const std::out_of_range &e) {
							}
							
						} else {
							try {
								RobloxPixelValue = static_cast<int>(((360.0f / CurrentSensValue) * (static_cast<float>(359) / 360)) + 0.5f);
							} catch (const std::invalid_argument &e) {
							} catch (const std::out_of_range &e) {
							}
						}
						PreviousSensValue = CurrentSensValue;
						sprintf(RobloxPixelValueChar, "%d", RobloxPixelValue);
					}

					ImGui::TextWrapped("Pixel Value for 180 Degree Turn BASED ON SENSITIVITY:");
					ImGui::SetNextItemWidth(90.0f);
					ImGui::SameLine();
					if (ImGui::InputText("##PixelValue", RobloxPixelValueChar, sizeof(RobloxPixelValueChar), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
						try {
							speed_strengthx = std::stoi(RobloxPixelValueChar);
							speed_strengthy = -std::stoi(RobloxPixelValueChar);
						} catch (const std::invalid_argument &e) {
						} catch (const std::out_of_range &e) {
						}
					}

					ImGui::Checkbox("Switch from Toggle Key to Hold Key", &isspeedswitch);
					ImGui::TextWrapped("This module abuses Roblox's conversion from angular velocity to regular velocity, and its flawed centre of mass calculation.");
					ImGui::Separator();
					ImGui::TextWrapped("IMPORTANT: Have your Sensitivity and Cam-Fix options set before using this module.");
					ImGui::Separator();
					ImGui::TextWrapped("Explanation:");
					ImGui::NewLine();

					ImGui::TextWrapped("Assuming unequip \"/e dance2\" is used prior to offset com, to activate a speed glitch, enable shiftlock mode "
										"(found in roblox settings), and press the keybind once to start the macro (or hold down if you are using the hold key option). "
										"Note that the macro should rotate you exactly 180 degrees. If not, verify your Roblox sensitivity in the settings matches the Macros sensitivity value, "
										"also, test out speedglitch with the \"Cam-Fix\" at the top left set to both true and false. Once the macro is activated, simply jump, and hold w. "
										"As long as you are in the air, you will start to gain immense velocity towards the direction you are facing (assuming shiftlock has been held, and "
										", and you are holding w). "
);
				}

				if (selected_section == 4) { // Gear Unequip COM offset
					ImGui::TextWrapped("Gear Slot:");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(30.0f);
					if (ImGui::InputText("##Gearslot", ItemSpeedSlot, sizeof(ItemSpeedSlot), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
						try {
							speed_slot = std::stoi(ItemSpeedSlot);
						} catch (const std::invalid_argument &e) {
						} catch (const std::out_of_range &e) {
						}
					}

					ImGui::TextWrapped("Type in a custom chat message! (Disables gear equipping, just pastes your message in chat)");
					ImGui::TextWrapped("(Leave this blank if you don't want a custom message)");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
					ImGui::InputText("##CustomText", CustomTextChar, sizeof(CustomTextChar));

					ImGui::SetNextItemWidth(150.0f);
					if (ImGui::BeginCombo("Select Emote", optionsforoffset[selected_dropdown])) {
						for (int i = 0; i < IM_ARRAYSIZE(optionsforoffset); i++) {
							bool is_selected = (selected_dropdown == i);
							if (ImGui::Selectable(optionsforoffset[i], is_selected)) {
								selected_dropdown = i;  // Update the selected option
								text = optionsforoffset[selected_dropdown];
							}
							if (is_selected) {
								ImGui::SetItemDefaultFocus();  // Ensure the selected item has focus
							}
						}
						ImGui::EndCombo();
					}

					// Get the binding state for vk_dkey
					BindingState& enterkeyState = g_bindingStates[&vk_enterkey];
    
					ImGui::AlignTextToFramePadding();
					ImGui::TextWrapped("Key to Press After Message/Emote paste:");
					ImGui::SameLine();
    
					if (ImGui::Button((enterkeyState.buttonText + "##EnterKey").c_str())) {
						enterkeyState.bindingMode = true;
						enterkeyState.notBinding = false;
						enterkeyState.buttonText = "Press a Key...";
					}
    
					ImGui::SameLine();
					vk_enterkey = BindKeyMode(&vk_enterkey, vk_enterkey, selected_section);
					ImGui::SetNextItemWidth(150.0f);
					GetKeyNameFromHex(vk_enterkey, enterkeyState.keyBufferHuman, sizeof(enterkeyState.keyBufferHuman));
					ImGui::InputText("Hex:", enterkeyState.keyBufferHuman, sizeof(enterkeyState.keyBufferHuman), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_ReadOnly);
					ImGui::SameLine();
					ImGui::SetNextItemWidth(50.0f);
					ImGui::PushID("Press2");
					// Use the state's keyBuffer for hex display
					ImGui::InputText("##HumanKey", enterkeyState.keyBuffer, sizeof(enterkeyState.keyBuffer), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CharsHexadecimal);
					ImGui::PopID();

					ImGui::Checkbox("Let the macro Keep the item equipped", &unequiptoggle);
					ImGui::Checkbox("Make Unequip Com only work while tabbed into Roblox", &unequipinroblox);

					ImGui::Separator();
					ImGui::TextWrapped("This module allows you to trick Roblox into thinking your centre of mass is elsewhere. This is used in the Helicopter "
										"High Jump, Speed Glitch and Walless LHJ modules (may change in the future).");
					ImGui::Separator();
					ImGui::TextWrapped("IMPORTANT: This ONLY works in R6. Although the glitch is possible in R15, the macro isn't built around that rig type. "
										"An Item is also required, and must be placed in the corresponding gear slot (3 by default).");
					ImGui::NewLine();
					ImGui::TextWrapped("Usage:");
					ImGui::TextWrapped("Assuming you have a gear ready, to get an offset com, put the gear into the corresponding gear slot (set above), "
										"and press the keybind (F8 is used with the fn key). Note if the emote bugs out such as restarting halfway through, "
										"or starting late due to a delay, your com may not be in its most offset state. Reusing the macro until the emote "
										"plays out error free will fix this.");
					ImGui::NewLine();
					ImGui::TextWrapped(
						"In most cases, you will be using the \"/e dance2\" emote, as that provides you with the furthest offset, although the other emotes "
						"are still useful occasionaly, such as \"/e laugh\" for wraparounds, and \"/e cheer\" for walless lhjs.");
				}

				if (selected_section == 5) { // Presskey / Press a Key
					// Get the binding state for vk_dkey
					BindingState& dkeyState = g_bindingStates[&vk_dkey];
    
					ImGui::TextWrapped("Key to Press:");
					ImGui::SameLine();
    
					if (ImGui::Button((dkeyState.buttonText + "##DKey").c_str())) {
						dkeyState.bindingMode = true;
						dkeyState.notBinding = false;
						dkeyState.buttonText = "Press a Key...";
					}
    
					ImGui::SameLine();
					vk_dkey = BindKeyMode(&vk_dkey, vk_dkey, selected_section);
					ImGui::SetNextItemWidth(150.0f);
					GetKeyNameFromHex(vk_dkey, dkeyState.keyBufferHuman, sizeof(dkeyState.keyBufferHuman));
					ImGui::InputText("Key to Press", dkeyState.keyBufferHuman, sizeof(dkeyState.keyBufferHuman), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_ReadOnly);
					ImGui::SameLine();
					ImGui::SetNextItemWidth(50.0f);
					ImGui::PushID("Press2");
					// Use the state's keyBuffer for hex display
					ImGui::InputText("Key to Press", dkeyState.keyBuffer, sizeof(dkeyState.keyBuffer), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CharsHexadecimal);
					ImGui::PopID();
					ImGui::NewLine();
					ImGui::SameLine(276);
					ImGui::Text("(Human-Readable)");
					ImGui::SameLine(510);
					ImGui::Text("(Hexadecimal)");

					ImGui::Text("Length of Second Button Press (ms):");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(80);
					if (ImGui::InputText("##PressKeyDelayChar", PressKeyDelayChar, sizeof(PressKeyDelayChar), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
						try {
							PressKeyDelay = std::stoi(PressKeyDelayChar);
						} catch (const std::invalid_argument &e) {
						} catch (const std::out_of_range &e) {
						}
					}

					ImGui::Text("Delay Before Second Press (ms):");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(80);
					if (ImGui::InputText("##PressKeyBonusDelayChar", PressKeyBonusDelayChar, sizeof(PressKeyBonusDelayChar), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
						try {
							PressKeyBonusDelay = std::stoi(PressKeyBonusDelayChar);
						} catch (const std::invalid_argument &e) {
						} catch (const std::out_of_range &e) {
						}
					}


					ImGui::Checkbox("Make PressKey only work while tabbed into Roblox", &presskeyinroblox);

					ImGui::Separator();
					ImGui::TextWrapped("Explanation:");
					ImGui::NewLine();
					ImGui::TextWrapped("It will press the second keybind for a single frame whenever you press the first keybind. "
										"This is most commonly used for micro-adjustments while moving, especially if you do this while jumping.");
				}

				if (selected_section == 6) { // Wallhop
					ImGui::TextWrapped("Flick Degrees (Estimated):");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(70.0f);
					float sensValue = std::atof(RobloxSensValue);
					if (sensValue != 0.0f) {
						snprintf(WallhopDegrees, sizeof(WallhopDegrees), "%d", static_cast<int>(360 * (std::atof(WallhopPixels) * std::atof(RobloxSensValue)) / (camfixtoggle ? 1000 : 720)));
					}
					
					if (ImGui::InputText("##WallhopDegrees", WallhopDegrees, sizeof(WallhopDegrees), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
						float pixels = std::atof(WallhopDegrees) * (camfixtoggle ? 1000.0f : 720.0f) / (360.0f * std::atof(RobloxSensValue));
						snprintf(WallhopPixels, sizeof(WallhopPixels), "%.0f", pixels);
						try {
							wallhop_dx = std::round(std::stoi(WallhopPixels));
							wallhop_dy = -std::round(std::stoi(WallhopPixels));
						} catch (const std::invalid_argument &e) {
						} catch (const std::out_of_range &e) {
						}
					}

					ImGui::TextWrapped("Flick Pixel Amount:");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(70.0f);
					if (ImGui::InputText("##WallhopPixels", WallhopPixels, sizeof(WallhopPixels), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
						try {
							wallhop_dx = std::round(std::stoi(WallhopPixels));
							wallhop_dy = -std::round(std::stoi(WallhopPixels));
						} catch (const std::invalid_argument &e) {
						} catch (const std::out_of_range &e) {
						}
					}

					ImGui::TextWrapped("Wallhop Length (ms):");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(70.0f);
					if (ImGui::InputText("##WallhopDelay", WallhopDelayChar, sizeof(WallhopDelayChar), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
						try {
							WallhopDelay = std::round(std::stoi(WallhopDelayChar));
						} catch (const std::invalid_argument &e) {
						} catch (const std::out_of_range &e) {
						}
					}

					ImGui::TextWrapped("Bonus Wallhop Delay Before Jumping (ms):");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(70.0f);
					if (ImGui::InputText("##WallhopBonusDelay", WallhopBonusDelayChar, sizeof(WallhopBonusDelayChar), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
						try {
							WallhopBonusDelay = std::round(std::stoi(WallhopBonusDelayChar));
						} catch (const std::invalid_argument &e) {
						} catch (const std::out_of_range &e) {
						}
					}

					ImGui::Checkbox("Switch to Left-Flick Wallhop", &wallhopswitch); // Left Sided wallhop switch
					ImGui::Checkbox("Jump During Wallhop", &toggle_jump);
					ImGui::Checkbox("Flick-Back During Wallhop", &toggle_flick);

					ImGui::Separator();
					ImGui::TextWrapped("IMPORTANT:");
					ImGui::TextWrapped("THE ANGLE THAT YOU TURN IS DIRECTLY RELATED TO YOUR ROBLOX SENSITIVITY. "
										"If you want to pick a SPECIFIC ANGLE, heres how. "
										"For games without the cam-fix module, 180 degrees is equal to 360 divided by your Roblox Sensitivity. "
										"For games with the cam-fix module, 180 degrees is equal to 500 divided by your Roblox Sensitivity. "
										"Ex: 0.6 sens with no cam fix = 600 pixels, which means 600 / 4 (150) is equal to a 45 degree turn.");
					ImGui::TextWrapped("INTEGERS ONLY!");
					ImGui::Separator();
					ImGui::TextWrapped("Explanation:");
					ImGui::NewLine();
					ImGui::TextWrapped("This Macro automatically flicks your screen AND jumps at the same time, performing a wallhop.");
				}

				if (selected_section == 7) { // Walless LHJ
					ImGui::Checkbox("Switch to Left-Sided LHJ", &wallesslhjswitch); // Left Sided lhj switch
					ImGui::Separator();
					ImGui::TextWrapped("Explanation:");
					ImGui::NewLine();
					ImGui::TextWrapped("If you offset your center of mass to any direction EXCEPT directly upwards, you will be able to perform "
										"14 stud jumps using this macro. However, you need at LEAST one FULL FOOT on the platform "
										"in order to do it.");
				}

				if (selected_section == 8) { // Item Clip
					ImGui::TextWrapped("Item Clip Slot:");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(30.0f);

					if (ImGui::InputText("##ItemClipSlot", ItemClipSlot, sizeof(ItemClipSlot), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
						try {
							clip_slot = std::stoi(ItemClipSlot);
						} catch (const std::invalid_argument &e) {
						} catch (const std::out_of_range &e) {
						}
					}

					ImGui::TextWrapped("Item Clip Delay in Milliseconds (Default 34ms):");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(120.0f);
					if (ImGui::InputText("##ItemClipDelay", ItemClipDelay, sizeof(ItemClipDelay), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
						try {
							clip_delay = std::stoi(ItemClipDelay);
						} catch (const std::invalid_argument &e) {
						} catch (const std::out_of_range &e) {
						}
					}

					ImGui::Checkbox("Switch from Toggle Key to Hold Key", &isitemclipswitch);

					ImGui::Separator();
					ImGui::TextWrapped("Explanation:");
					ImGui::NewLine();
					ImGui::TextWrapped("This macro will equip and unequip your item in the amount of milliseconds you put in. "
										"It's recommended to shiftlock, jump, and hold W while staying at the wall. "
										"This lets you clip through walls in both R6 and R15, however, it is EXTREMELY RNG. "
										"There are way too factors that control this, the delay, fps, the item's size, your animation, etc. "
										"The item in the best scenario should be big and stretch far into the wall. ");
					ImGui::TextWrapped("Also, for convenience sake, you cannot activate item clip unless you're tabbed into roblox.");
				}

				if (selected_section == 9) { // Laugh Clip
					ImGui::Checkbox("Disable S being pressed (Slightly weaker laugh clips, but interferes with movement less)", &laughmoveswitch);
					ImGui::Checkbox("Replace Shiftlock with Zooming In", &laughzoomin);
					ImGui::SameLine();
					ImGui::Checkbox("Reverse Direction?", &laughzoominreverse);
					ImGui::Separator();
					ImGui::TextWrapped("Explanation:");
					ImGui::NewLine();
					ImGui::TextWrapped("MUST BE ABOVE 60 FPS AND IN R6!");
					ImGui::TextWrapped("Go against a wall unshiftlocked and angle your camera DIRECTLY OPPOSITE TO THE WALL. "
										"The Macro will Automatically type out /e laugh using the settings inside of the \"Unequip Com\" section. "
										"It will automatically time your shiftlock and jump to laugh clip through up to ~1.3 studs.");
				}

				if (selected_section == 10) { // Wall-Walk

					float CurrentWallWalkValue = atof(RobloxSensValue);
					float CurrentWallwalkSide = camfixtoggle;


					if (CurrentWallWalkValue != PreviousWallWalkValue) {
						if (camfixtoggle) {
							wallwalk_strengthx = static_cast<int>(round((500.0f / CurrentWallWalkValue) * 0.13f));
							wallwalk_strengthy = -static_cast<int>(round((500.0f / CurrentWallWalkValue) * 0.13f));
						} else {
							wallwalk_strengthx = static_cast<int>(round((360.0f / CurrentWallWalkValue) * 0.13f));
							wallwalk_strengthy = -static_cast<int>(round((360.0f / CurrentWallWalkValue) * 0.13f));
						}
					}

					PreviousWallWalkValue = CurrentWallWalkValue;
					sprintf(RobloxWallWalkValueChar, "%d", wallwalk_strengthx);

					ImGui::TextWrapped("Wall-Walk Pixel Value BASED ON SENSITIVITY (meant to be low):");
					ImGui::SetNextItemWidth(90.0f);
					ImGui::SameLine();
					ImGui::InputText("##PixelValue", RobloxWallWalkValueChar, sizeof(RobloxWallWalkValueChar), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank);

					ImGui::Checkbox("Switch to Left-Flick Wallwalk", &wallwalktoggleside);

					ImGui::SetNextItemWidth(100.0f);
					ImGui::InputText("Delay Between Flicks (Don't change from 72720 unless neccessary):", RobloxWallWalkValueDelayChar, sizeof(RobloxWallWalkValueDelayChar), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank);

					try {
						RobloxWallWalkValueDelay = atof(RobloxWallWalkValueDelayChar);
					} catch (const std::invalid_argument &e) {
					} catch (const std::out_of_range &e) {
					}

					try { // Error Handling
						wallwalk_strengthx = std::stoi(RobloxWallWalkValueChar);
						wallwalk_strengthy = -std::stoi(RobloxWallWalkValueChar);
					} catch (const std::invalid_argument &e) {
					} catch (const std::out_of_range &e) {
					}

					ImGui::Checkbox("Switch from Toggle Key to Hold Key", &iswallwalkswitch);
					ImGui::Separator();
					ImGui::TextWrapped("IMPORTANT: FOR MOST OPTIMAL RESULTS, INPUT YOUR ROBLOX INGAME SENSITIVITY!");
					ImGui::TextWrapped("THE HIGHER FPS YOU ARE, THE MORE STABLE IT GETS, HOWEVER 60 FPS IS ENOUGH FOR INFINITE DISTANCE");
					ImGui::TextWrapped("TICK OR UNTICK THE CHECKBOX DEPENDING ON WHETHER THE GAME USES CAM-FIX MODULE OR NOT. "
										"If you don't know, do BOTH and check which one provides you with a 180 degree rotation. "
										"You can also toggle whether it's right facing or left facing (Makes its respective side easier) "
										"Also, for convenience sake, you cannot activate speedglitch unless you're tabbed into roblox.");
					ImGui::Separator();
					ImGui::TextWrapped("Explanation:");
					ImGui::NewLine();
					ImGui::TextWrapped("This macro abuses the way leg raycast physics work to permanently keep wallhopping, without jumping "
										"you can walk up to a wall, maybe at a bit of an angle, and hold W and D or A to slowly walk across.");
			}

				if (selected_section == 11) { // Spamkey
					BindingState& spamState = g_bindingStates[&vk_spamkey];
    
					ImGui::TextWrapped("Key to Press:");
					ImGui::SameLine();
					if (ImGui::Button((spamState.buttonText + "##").c_str())) {
						spamState.bindingMode = true;
						spamState.notBinding = false;
						spamState.buttonText = "Press a Key...";
					}
					ImGui::SameLine();
					vk_spamkey = BindKeyMode(&vk_spamkey, vk_spamkey, selected_section);
					ImGui::SetNextItemWidth(150.0f);
					GetKeyNameFromHex(vk_spamkey, spamState.keyBufferHuman, sizeof(spamState.keyBufferHuman));
					ImGui::InputText("Key to Press", spamState.keyBufferHuman, sizeof(spamState.keyBufferHuman), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_ReadOnly);
					ImGui::SameLine();
					ImGui::SetNextItemWidth(50.0f);
					ImGui::PushID("Press2");
					ImGui::InputText("Key to Press", spamState.keyBuffer, sizeof(spamState.keyBuffer), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CharsHexadecimal);
					ImGui::PopID();
					ImGui::NewLine();
					ImGui::SameLine(276);
					ImGui::Text("(Human-Readable)");
					ImGui::SameLine(510);
					ImGui::Text("(Hexadecimal)");
					ImGui::TextWrapped("Spam Delay (Milliseconds):");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(120.0f);
					if (ImGui::InputText("##SpamDelay", SpamDelay, sizeof(SpamDelay), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
						try {
						spam_delay = std::stof(SpamDelay);
						real_delay = static_cast<int>((spam_delay + 0.5f) / 2);
						} catch (const std::invalid_argument &e) {
						} catch (const std::out_of_range &e) {
						}
					}

					ImGui::TextWrapped("I do not take any responsibility if you set the delay to 0ms");
					ImGui::Checkbox("Switch from Toggle Key to Hold Key", &isspamswitch);
					ImGui::Separator();
					ImGui::TextWrapped("Explanation:");
					ImGui::NewLine();
					ImGui::TextWrapped("This macro will spam the second key with a millisecond delay. "
										"This can be used as an autoclicker for any games you want, or a key-spam.");
				}

				if (selected_section == 12) { // Ledge Bounce
					ImGui::Checkbox("Switch Ledge Bounce to Left-Sided", &bouncesidetoggle);
					ImGui::Checkbox("Stay Horizontal After Bounce", &bouncerealignsideways);
					ImGui::Checkbox("Automatically Hold Movement Keys", &bounceautohold);
					ImGui::Separator();
					ImGui::TextWrapped("IMPORTANT:");
					ImGui::TextWrapped("PLEASE SET YOUR ROBLOX SENS AND CAM-FIX CORRECTLY SO IT CAN ACTUALLY DO THE PROPER TURNS! It works best at high FPS (120+).");
					ImGui::TextWrapped("Also, if you set it to automatically hold movement keys, PLEASE HOLD THE KEY AT THE PROPER TIME, else it will keep moving forever.");
					ImGui::Separator();
					ImGui::TextWrapped("Explanation:");
					ImGui::NewLine();
					ImGui::TextWrapped(
									"Walk up to a ledge with your camera sideways, about half of your left foot should be on the platform. "
									"The Macro will Automatically flick your camera 90 degrees, let you fall, and then flick back. "
									"This will boost you up slightly into the air, and you can even jump after it. This lets you \"jump\" without jumping.");
				}

				if (selected_section == 13) { // Bunnyhop
					ImGui::TextWrapped("Bunnyhop Delay in Milliseconds (Default 10ms):");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(120.0f);
					if (ImGui::InputText("##BunnyhopDelay", BunnyHopDelayChar, sizeof(BunnyHopDelayChar), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
						try {
							BunnyHopDelay = atof(BunnyHopDelayChar);
						} catch (const std::invalid_argument &e) {
						} catch (const std::out_of_range &e) {
						}
					}

					ImGui::Checkbox("Enable Intelligent Auto-Toggle", &bunnyhopsmart);

					ImGui::Separator();
					ImGui::TextWrapped("If Intelligent Auto-Toggle is on, pressing your chat key will temporarily disable bhop "
									   "until you press left click or enter to leave the chat.");
					ImGui::Separator();

					ImGui::TextWrapped("Explanation:");
					ImGui::NewLine();
					ImGui::TextWrapped(
									"This Macro will automatically spam your key (typically space) with a specified delay whenever space is held down. "
									"This is created as a more functional Spamkey implementation specifically for Bhop/Bunnyhop.");

					ImGui::TextWrapped("This will not be active unless you are currently inside of the target program.");
				}

            } else {
                ImGui::TextWrapped("Select a section to see its settings.");
            }

			ImVec2 childpos = ImGui::GetWindowPos();
			ImVec2 childsize = ImGui::GetWindowSize();
			ImU32 lineColor = ImGui::GetColorU32(ImGuiCol_Border);


            ImGui::EndChild(); // End right section

			ImDrawList *draw_list = ImGui::GetWindowDrawList();

			// Draw a line at the bottom to remove the border, color it the same as the background
			ImGui::GetWindowDrawList()->AddLine(
				ImVec2(childpos.x, childpos.y + childsize.y - 1), 
				ImVec2(childpos.x + childsize.x, childpos.y + childsize.y - 1), 
				IM_COL32(21, 22, 23, 255),
				1.0f
			);

			// Add in an extra line to the bottom left to fill missing pixels
			draw_list->AddLine(
				ImVec2(childpos.x, childpos.y + childsize.y - 1),
				ImVec2(childpos.x, childpos.y + childsize.y + 29),
				lineColor,
				1.0f
			);

			// Add in an extra line to the bottom right to fill missing pixels
			draw_list->AddLine(
				ImVec2(childpos.x + childsize.x - 1, childpos.y + childsize.y - 1),
				ImVec2(childpos.x + childsize.x - 1, childpos.y + childsize.y + 29),
				lineColor,
				1.0f
			);

			// Draw in a fake line to add in a new border
			ImGui::GetWindowDrawList()->AddLine(
				ImVec2(childpos.x, childpos.y + childsize.y + 29), 
				ImVec2(childpos.x + childsize.x, childpos.y + childsize.y + 29), 
				lineColor,
				1.0f
			);


			// Draw bottom controls
			ImVec2 windowSize = ImGui::GetWindowSize();
			ImGui::SetCursorPosY(windowSize.y - 27 - ImGui::GetStyle().WindowPadding.y);
			ImGui::SetCursorPosX(windowSize.x - 624);

			if (ImGui::BeginChild("BottomControls", ImVec2(0, 36), false,
				ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
			{
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Always On-Top");
				ImGui::SameLine();
		
				if (ImGui::Checkbox("##OnTopToggle", &ontoptoggle))
				{
					SetWindowPos(hwnd, ontoptoggle ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				}

				ImGui::SameLine();
				ImGui::Text("Opacity");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(100.0f);

				if (ImGui::SliderFloat("##OpacitySlider", &windowOpacityPercent, 20.0f, 100.0f, "%.0f%%"))
				{
					BYTE alpha = static_cast<BYTE>((windowOpacityPercent / 100.0f) * 255);
					SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
				}
				ImGui::SameLine();

				ProfileUI::DrawProfileManagerUI();
			}
			ImGui::EndChild();

            // Finish the main window
            ImGui::End(); // End main ImGui window

            // Render
            ImGui::Render();
            const float clear_color[] = { 0.45f, 0.55f, 0.60f, 1.00f };
            g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
            g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            g_pSwapChain->Present(1, 0);  // Vsync present for smooth rendering
			// Wait until next frame
			std::this_thread::sleep_until(nextFrameTime);

        }

        // No rendering needed
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    UnregisterClass(wc.lpszClassName, wc.hInstance);
    AttachThreadInput(mainThreadId, guiThreadId, FALSE);
}

void DbgPrintf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); // Use vsnprintf for safety
    va_end(args);
    OutputDebugStringA(buffer); // Send to debug output
}

void CreateDebugConsole() {
    if (AllocConsole()) {
        FILE* pCout;
        freopen_s(&pCout, "CONOUT$", "w", stdout); // Redirect stdout
        FILE* pCerr;
        freopen_s(&pCerr, "CONOUT$", "w", stderr); // Redirect stderr
        FILE* pCin;
        freopen_s(&pCin, "CONIN$", "r", stdin);   // Redirect stdin

        // Optional: Set console title
        SetConsoleTitle(L"Debug Console");

        std::cout.sync_with_stdio(true);
    }
}

// START OF CODE THREAD

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    DisablePowerThrottling();

	// Create Debug Console, use DbgPrintf, printf, or cout to use
    char debugbuffer[16];
    if (GetEnvironmentVariableA("DEBUG", debugbuffer, sizeof(debugbuffer)) && std::string(debugbuffer) == "1") {
        CreateDebugConsole();
    } else {
		// CreateDebugConsole();
	}

	// Run timers with max precision
    timeBeginPeriod(1);

	// I LOVE THREAD PRIORITY!!!!!!!!!!!!!!!
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	// Check for any updates
    std::string remoteVersion = GetRemoteVersion();

    if (!remoteVersion.empty()) 
    {
        remoteVersion = Trim(remoteVersion);
        std::string localVersion = "3.1.0";

        if (remoteVersion != localVersion) 
        {
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
            std::wstring wRemoteVersion = converter.from_bytes(remoteVersion);
            std::wstring wLocalVersion = converter.from_bytes(localVersion);
            
            std::wstring message = L"An update is available!\n\n"
                                   L"Current version: " + wLocalVersion + L"\n"
                                   L"Latest version:  " + wRemoteVersion + L"\n\n"
                                   L"Would you like to update now?";

            int result = MessageBoxW(NULL,
                message.c_str(),
                L"Update Available",
                MB_YESNOCANCEL | MB_ICONINFORMATION | MB_DEFBUTTON1 | MB_APPLMODAL);

            if (result == IDYES) 
            {
				PerformUpdate(remoteVersion, localVersion);
            }

			if (result == IDNO) 
			{
				UserOutdated = true;
			}
		
			if (result == IDCANCEL) 
			{
				exit(0);
			}
        }
    }

	// Setup suspension

	std::thread actionThread(Speedglitchloop); // Start a separate thread for item desync loop, lets functions run alongside
	std::thread actionThread2(ItemDesyncLoop);
	std::thread actionThread3(SpeedglitchloopHHJ);
	std::thread actionThread4(SpamKeyLoop);
	std::thread actionThread5(ItemClipLoop);
	std::thread actionThread6(WallWalkLoop);
	std::thread actionThread7(BhopLoop);
	std::thread actionThread8(WallhopThread);
	std::thread actionThread9(PressKeyThread);
	
	std::thread guiThread(RunGUI);

	std::thread KeyboardThread;
	if (!g_isLinuxWine) {
		KeyboardThread = std::thread(KeyboardHookThread);
	}

	MSG msg;

	std::vector<DWORD> targetPIDs = FindProcessIdsByName_Compat(settingsBuffer, takeallprocessids);

    if (targetPIDs.empty()) {
        processFound = false;
    } else {
		processFound = true;
	}

	std::vector<HANDLE> hProcess = GetProcessHandles_Compat(targetPIDs, PROCESS_SUSPEND_RESUME | PROCESS_QUERY_LIMITED_INFORMATION);
	std::vector<HWND> rbxhwnd = FindWindowByProcessHandle(hProcess); // SET ROBLOX WINDOW HWND RAHHHHH

	// These variables are used for "one-click" functionalies for macros, so you can press a key and it runs every time that key is pressed (without overlapping itself)
	bool isdesync = false;
	bool isSuspended = false; 
	bool islhj = false;
	bool ispresskey = false;
	bool iswallhop = false;
	bool isspeedglitch = false;
	bool isunequipspeed = false;
	bool HHJ = false;
	bool isspam = false;
	bool isclip = false;
	bool iswallwalk = false;
	bool islaugh = false;
	bool isbounce = false;
	bool isbhop = false;
	bool bhoplocked = false;
	static const float targetFrameTime = 1.0f / 90.0f; // Targeting 90 FPS
	auto lastPressTime = std::chrono::steady_clock::now();
	auto lastProcessCheck = std::chrono::steady_clock::now();
	auto startTime = std::chrono::steady_clock::now();
	auto now = std::chrono::steady_clock::now();
	auto processchecktime = std::chrono::steady_clock::now();
	static int counter = 0;

	while (!done) {
	bool tabbedintoroblox = IsForegroundWindowProcess(hProcess);
    {
		tabbedintoroblox = IsForegroundWindowProcess(hProcess);
		// Freeze
		if ((macrotoggled && notbinding && section_toggles[0])) {
			bool isMButtonPressed = IsKeyPressed(vk_mbutton);

			if (isfreezeswitch) {  // Toggle mode
				if (isMButtonPressed && !wasMButtonPressed && (freezeoutsideroblox || tabbedintoroblox)) {  // Detect button press edge
					isSuspended = !isSuspended;  // Toggle the freeze state
					SuspendOrResumeProcesses_Compat(targetPIDs, hProcess, isSuspended);

					if (isSuspended) {
						suspendStartTime = std::chrono::steady_clock::now();  // Start the timer
					}
				}
			} else {  // Hold mode
				if (isMButtonPressed && (freezeoutsideroblox || tabbedintoroblox)) {
					if (!isSuspended) {
						SuspendOrResumeProcesses_Compat(targetPIDs, hProcess, true);  // Freeze on hold
						isSuspended = true;
						suspendStartTime = std::chrono::steady_clock::now();  // Start the timer
					}
				} else if (isSuspended) {
					SuspendOrResumeProcesses_Compat(targetPIDs, hProcess, false);  // Unfreeze on release
					isSuspended = false;
				}
			}

			// Common timer logic for both toggle and hold modes
			if (isSuspended) {
				auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - suspendStartTime).count();

				if (elapsed >= (maxfreezetime * 1000)) {
					// Unsuspend for 50 ms
					SuspendOrResumeProcesses_Compat(targetPIDs, hProcess, false);
					std::this_thread::sleep_for(std::chrono::milliseconds(maxfreezeoverride));
					SuspendOrResumeProcesses_Compat(targetPIDs, hProcess, true);

					// Reset the timer
					suspendStartTime = std::chrono::steady_clock::now();
				}
			}

			// Update the previous state
			wasMButtonPressed = isMButtonPressed;
		}

		// Item Desync Macro with anti-idiot design
		if (IsKeyPressed(vk_f5) && tabbedintoroblox && macrotoggled && notbinding && section_toggles[1]) {
			if (!isdesync) {
				isdesyncloop.store(true, std::memory_order_relaxed);
				isdesync = true;
			}
		} else {
			isdesync = false;
			isdesyncloop.store(false, std::memory_order_relaxed);
		}

		// PressKey
		if (IsKeyPressed(vk_zkey) && macrotoggled && notbinding && section_toggles[5] && (!presskeyinroblox || tabbedintoroblox)) {
			if (!ispresskey) {
				ispresskeythread.store(true, std::memory_order_relaxed);
				ispresskey = true;
			}
		} else {
			ispresskey = false;
			ispresskeythread.store(false, std::memory_order_relaxed);
		}

		// Wallhop (Ran in separate thread)
		if (IsKeyPressed(vk_xbutton2) && macrotoggled && notbinding && section_toggles[6]) {
			if (!iswallhop) {
				iswallhopthread.store(true, std::memory_order_relaxed);
				iswallhop = true;
				}
		} else {
			iswallhopthread.store(false, std::memory_order_relaxed);
			iswallhop = false;
		}

		// Walless LHJ (REQUIRES COM OFFSET AND .5 STUDS OF A FOOT ON A PLATFORM)
		if (IsKeyPressed(vk_f6) && macrotoggled && notbinding && section_toggles[7]) {
			if (!islhj) {
				if (wallesslhjswitch) {
					HoldKey(0x1E);
				} else {
					HoldKey(0x20);
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(15));
				HoldKey(0x39);
				std::this_thread::sleep_for(std::chrono::milliseconds(30));
				SuspendOrResumeProcesses_Compat(targetPIDs, hProcess, true);
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				if (wallesslhjswitch) {
					ReleaseKey(0x1E);
				} else {
					ReleaseKey(0x20);
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(500));

				if (!lhjzoomin) {
					HoldKeyBinded(vk_shiftkey);
				} else {
					INPUT mousezoominput = {0};
					mousezoominput.type = INPUT_MOUSE;
					mousezoominput.mi.dwFlags = MOUSEEVENTF_WHEEL;
					mousezoominput.mi.mouseData = lhjzoominreverse ? WHEEL_DELTA * -100 : WHEEL_DELTA * 100;

					SendInput(1, &mousezoominput, sizeof(INPUT));
				}
				SuspendOrResumeProcesses_Compat(targetPIDs, hProcess, false);
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				if (!lhjzoomin) {
					ReleaseKeyBinded(vk_shiftkey);
				}

				ReleaseKey(0x39);
				islhj = true;
			}
		} else {
			islhj = false;
		}

		// Speedglitch
		if (IsKeyPressed(vk_xkey) && tabbedintoroblox && macrotoggled && notbinding && section_toggles[3]) {
			if (!isspeedglitch) {
				isspeed = !isspeed;
				isspeedglitch = true;
			}
		} else {
			isspeedglitch = false;
			if (isspeedswitch) {
				isspeed = false;
			}
		}

		// Gear Unequip COM Offset
		if (IsKeyPressed(vk_f8) && macrotoggled && notbinding && section_toggles[4] && (!unequipinroblox || tabbedintoroblox)) {
			if (!isunequipspeed) {
				if (chatoverride) {
					HoldKey(0x35);
				} else {
					HoldKeyBinded(vk_chatkey);
					std::this_thread::sleep_for(std::chrono::milliseconds(17));
					ReleaseKeyBinded(vk_chatkey);
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(17));
				if (chatoverride) {
					ReleaseKey(0x35);
				}

				if (strlen(CustomTextChar) >= 1) {
					PasteText(CustomTextChar);
					std::this_thread::sleep_for(std::chrono::milliseconds(25));
					HoldKeyBinded(vk_enterkey);

					std::this_thread::sleep_for(std::chrono::milliseconds(35));
					ReleaseKeyBinded(vk_enterkey);
					isunequipspeed = true;
					continue;
				}

				PasteText(text);
				std::this_thread::sleep_for(std::chrono::milliseconds(25));
				HoldKeyBinded(vk_enterkey);

				std::this_thread::sleep_for(std::chrono::milliseconds(35));
				ReleaseKeyBinded(vk_enterkey);
				if (selected_dropdown == 2) { // Cheer
					std::this_thread::sleep_for(std::chrono::milliseconds(16));
				} else {
					std::this_thread::sleep_for(std::chrono::milliseconds(65));
				}

				if (selected_dropdown == 0) { // Dance
					std::this_thread::sleep_for(std::chrono::milliseconds(815));
				}
				if (selected_dropdown == 1) { // Laugh
					std::this_thread::sleep_for(std::chrono::milliseconds(175));
				}
				HoldKey(speed_slot + 1);
				std::this_thread::sleep_for(std::chrono::milliseconds(4));
				ReleaseKey(speed_slot + 1);
				std::this_thread::sleep_for(std::chrono::milliseconds(4));
				if (!unequiptoggle) {
					HoldKey(speed_slot + 1);
				}
				ReleaseKey(speed_slot + 1);
				isunequipspeed = true;
			}
		} else {
			isunequipspeed = false;
		}

		// Helicopter High jump
		if (IsKeyPressed(vk_xbutton1) && macrotoggled && notbinding && section_toggles[2]) {
			if (!HHJ) {

				if (autotoggle) { // Auto-Key-Timer
					HoldKey(0x39);
					std::this_thread::sleep_for(std::chrono::milliseconds(550));
					HoldKey(0x11);
					std::this_thread::sleep_for(std::chrono::milliseconds(68));
				}

				SuspendOrResumeProcesses_Compat(targetPIDs, hProcess, true);

				// If the users custom freeze duration is zero or nothing, do regular stuff, else, use their custom duration
				if (strcmp(HHJFreezeDelayOverrideChar, "0") == 0 || strcmp(HHJFreezeDelayOverrideChar, "") == 0) {
					// FastHHJ lowers freeze time by 300 ms
					if (!fasthhj) {
						std::this_thread::sleep_for(std::chrono::milliseconds(300));
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(200));
				} else {
					std::this_thread::sleep_for(std::chrono::milliseconds(HHJFreezeDelayOverride));
				}

				if (autotoggle) { // Auto-Key-Timer
					ReleaseKey(0x39);
					ReleaseKey(0x11);
				}

				SuspendOrResumeProcesses_Compat(targetPIDs, hProcess, false);

				std::this_thread::sleep_for(std::chrono::milliseconds(HHJDelay1));
				if (!hhjzoomin) {
					HoldKeyBinded(vk_shiftkey);
				} else {
					INPUT mousezoominput = {0};
					mousezoominput.type = INPUT_MOUSE;
					mousezoominput.mi.dwFlags = MOUSEEVENTF_WHEEL;
					mousezoominput.mi.mouseData = hhjzoominreverse ? WHEEL_DELTA * -100 : WHEEL_DELTA * 100;

					SendInput(1, &mousezoominput, sizeof(INPUT));
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(HHJDelay2));
				isHHJ.store(true, std::memory_order_relaxed);
				std::this_thread::sleep_for(std::chrono::milliseconds(HHJDelay3));
				if (!hhjzoomin) {
					ReleaseKeyBinded(vk_shiftkey);
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(HHJLength));
				isHHJ.store(false, std::memory_order_relaxed);
				HHJ = true;
			}
		} else {
			HHJ = false;
		}

		// Spamkey
		if (IsKeyPressed(vk_leftbracket) && macrotoggled && notbinding && section_toggles[11]) {
			if (!isspam) {
				isspamloop = !isspamloop;
				isspam = true;
			}
		} else {
			isspam = false;
			if (isspamswitch) {
				isspamloop = false;
			}
		}

		// Laughkey
		if (IsKeyPressed(vk_laughkey) && tabbedintoroblox && macrotoggled && notbinding && section_toggles[9]) {
			if (!islaugh) {
				if (chatoverride) {
					HoldKey(0x35);
				} else {
					HoldKeyBinded(vk_chatkey);
					std::this_thread::sleep_for(std::chrono::milliseconds(17));
					ReleaseKeyBinded(vk_chatkey);
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				if (chatoverride) {
					ReleaseKey(0x35);
				}

				PasteText("/e laugh");
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				HoldKey(0x1C);

				std::this_thread::sleep_for(std::chrono::milliseconds(35));
				ReleaseKey(0x1C);

				std::this_thread::sleep_for(std::chrono::milliseconds(248));

				HoldKey(0x39); // Jump
				if (!laughzoomin) {
					HoldKeyBinded(vk_shiftkey);
				} else {
					INPUT mousezoominput = {0};
					mousezoominput.type = INPUT_MOUSE;
					mousezoominput.mi.dwFlags = MOUSEEVENTF_WHEEL;
					mousezoominput.mi.mouseData = laughzoominreverse ? WHEEL_DELTA * -100 : WHEEL_DELTA * 100;

					SendInput(1, &mousezoominput, sizeof(INPUT));
				}

				if (!laughmoveswitch) {
					HoldKey(0x1F); // S
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(25));
				ReleaseKey(0x39);

				if (!laughzoomin) {
					ReleaseKeyBinded(vk_shiftkey);
				}
				ReleaseKey(0x1C);
				std::this_thread::sleep_for(std::chrono::milliseconds(25));

				if (!laughmoveswitch) {
					ReleaseKey(0x1F); // S
				}

				islaugh = true;
			}
		} else {
			islaugh = false;
		}


		// Ledge Bounce
		if (IsKeyPressed(vk_bouncekey) && tabbedintoroblox && macrotoggled && notbinding && section_toggles[12]) {
			if (!isbounce) {
				int turn90 = (camfixtoggle ? 250 : 180) / atof(RobloxSensValue);
				int skey = 0x1F; // S key
				int dkey = 0x20; // D key
				int wkey = 0x11; // W key

				if (bouncesidetoggle) {
					turn90 = -turn90;
					dkey = 0x1E; // A Key
				}

				MoveMouse(-turn90, 0); // Turn Left
				std::this_thread::sleep_for(std::chrono::milliseconds(90));
				HoldKey(skey); // Hold S
				std::this_thread::sleep_for(std::chrono::milliseconds(40));
				ReleaseKey(skey); // Release S
				MoveMouse(turn90, 0); // Turn Right
				HoldKey(dkey);    // Hold D


				std::this_thread::sleep_for(std::chrono::milliseconds(16));

				if (bouncerealignsideways) {
					ReleaseKey(dkey); // Release D
					HoldKey(wkey); // Hold W
				}

				if (!bouncerealignsideways) {
					ReleaseKey(dkey);
				}

				// After Bounce
				if (bouncerealignsideways) {
					MoveMouse(turn90, 0); // Turn Right to face towards ledge
					std::this_thread::sleep_for(std::chrono::milliseconds(70));
					ReleaseKey(wkey);      // Release W
					MoveMouse(-turn90, 0); // Turn Left to face normally
					// Right facing end
					if (bounceautohold) {
						HoldKey(dkey); // Hold D
					}
				} else {
					// Front Facing End
					if (bounceautohold) {
						HoldKey(wkey); // Hold W
					}
					MoveMouse(turn90, 0); // Turn Right
				}

				isbounce = true;
			}
		} else {
			isbounce = false;
		}


		// Item Clip
		if (IsKeyPressed(vk_clipkey) && tabbedintoroblox && macrotoggled && notbinding && section_toggles[8]) {
			if (!isclip) {
				isitemloop = !isitemloop;
				isclip = true;
			}
		} else {
			isclip = false;
			if (isitemclipswitch) {
				isitemloop = false;
			}
		}


		// WallWalk
		if (IsKeyPressed(vk_wallkey) && tabbedintoroblox && macrotoggled && notbinding && section_toggles[10]) {
			if (!iswallwalk) {
				iswallwalkloop = !iswallwalkloop;
				iswallwalk = true;
			}
		} else {
			iswallwalk = false;
			if (iswallwalkswitch) {
				iswallwalkloop = false;
			}
		}

		bool can_process_bhop = IsKeyPressed(vk_bunnyhopkey) && tabbedintoroblox && section_toggles[13] && macrotoggled && notbinding;
		std::cout << "Can Process Bhop: " << can_process_bhop << std::endl;
		if (IsKeyPressed(vk_chatkey)) {
			bhoplocked = true;
		}

		if (bhoplocked) {
			if (IsKeyPressed(VK_RETURN) || IsKeyPressed(VK_LBUTTON)) {
				bhoplocked = false;
			}
		}

		if (can_process_bhop) {
			bool is_bunnyhop_key_considered_held = g_isVk_BunnyhopHeldDown.load(std::memory_order_relaxed) || IsKeyPressed(vk_bunnyhopkey);

			if (bunnyhopsmart) {
				if (!bhoplocked && is_bunnyhop_key_considered_held) {
					if (!isbhop) {
						isbhoploop = true;
						isbhop = true;
					}
				} else {
					if (isbhop) {
						isbhoploop = false;
						isbhop = false;
					}
				}
			} else {
				if (is_bunnyhop_key_considered_held) {
					if (!isbhop) {
						isbhoploop = true;
						isbhop = true;
					}
				} else {
					if (isbhop) {
						isbhoploop = false;
						isbhop = false;
					}
				}
			}
		}

		// Every second, check if roblox continues to exist.
		if (++counter % 100 == 0) {  // Check time every 100th iteration
			now = std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::seconds>(now - processchecktime).count() >= 1) {

				targetPIDs = FindProcessIdsByName_Compat(settingsBuffer, takeallprocessids);

				if (targetPIDs.empty()) {
					processFound = false;
				} else {
					processFound = true;
				}

				hProcess = GetProcessHandles_Compat(targetPIDs, PROCESS_SUSPEND_RESUME | PROCESS_QUERY_LIMITED_INFORMATION);
				rbxhwnd = FindWindowByProcessHandle(hProcess);
				counter = 0;
				processchecktime = std::chrono::steady_clock::now();
			}
		}

		// Anti AFK (MUST STAY AT THE LOWEST PART OF THE LIST!!!)
		if ((!isafk && tabbedintoroblox) || !antiafktoggle) {
			// Not Afk, reset lastpresstime
			lastPressTime = std::chrono::steady_clock::now();
		} else {
			if (processFound && antiafktoggle && isafk && !g_isLinuxWine) {
				// Check if AntiAFKTime has passed
				auto elapsedMinutes = std::chrono::duration_cast<std::chrono::minutes>(now - lastPressTime).count();

				if (elapsedMinutes >= AntiAFKTime) {
					HWND originalHwnd = GetForegroundWindow();
					// Not tabbed into Roblox AntiAFK
					if (!IsForegroundWindowProcess(hProcess)) {
						// Save User Mouse Position to return to later
						POINT originalMousePos;
						GetCursorPos(&originalMousePos);

						HWND windowhwnd = FindNewestProcessWindow(rbxhwnd);
						SetWindowPos(windowhwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
						SetWindowPos(windowhwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

						// Get roblox rectangle coords
						RECT robloxRect;
						if (!GetWindowRect(windowhwnd, &robloxRect)) {
							std::cerr << "Failed to get window rect" << std::endl;
							continue; // or break depending on your loop
						}

						// Calculate center coordinates
						int centerX = robloxRect.left + (robloxRect.right - robloxRect.left) / 2;
						int centerY = robloxRect.top + (robloxRect.bottom - robloxRect.top) / 2;

						Sleep(300);

						// Move mouse to center of the wanted window using SetCursorPos
						SetCursorPos(centerX, centerY);

						// Click and Unclick LMB to get window focus
						INPUT mclickinput = {0};
						mclickinput.type = INPUT_MOUSE;
						mclickinput.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
						SendInput(1, &mclickinput, sizeof(INPUT));
						mclickinput.mi.dwFlags = MOUSEEVENTF_LEFTUP;
						Sleep(50);
						SendInput(1, &mclickinput, sizeof(INPUT));

						Sleep(500);

						if (doublepressafkkey) {
							HoldKeyBinded(vk_afkkey);
							Sleep(20);
							ReleaseKeyBinded(vk_afkkey);
							Sleep(20);
							HoldKeyBinded(vk_afkkey);
							Sleep(20);
							ReleaseKeyBinded(vk_afkkey);
						} else {
							HoldKeyBinded(vk_afkkey);
							Sleep(20);
							ReleaseKeyBinded(vk_afkkey);
						}
						Sleep(200);
						SetWindowPos(originalHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
						SetWindowPos(originalHwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
						Sleep(50);
						SetForegroundWindow(originalHwnd);
						Sleep(1000);

						SetCursorPos(originalMousePos.x, originalMousePos.y);

						lastPressTime = std::chrono::steady_clock::now();
					}
					// Pre Tabbed into Roblox
					if (IsForegroundWindowProcess(hProcess) && !g_isLinuxWine) {
						if (chatoverride) {
							HoldKey(0x35);
						} else {
							HoldKeyBinded(vk_chatkey);
							std::this_thread::sleep_for(std::chrono::milliseconds(17));
							ReleaseKeyBinded(vk_chatkey);
						}
						if (chatoverride) {
							Sleep(20);
							ReleaseKey(0x35);
						}
						Sleep(20);
						HoldKey(0x34);
						Sleep(20);
						ReleaseKey(0x34);
						Sleep(20);
						HoldKey(0x1C);
						Sleep(20);
						ReleaseKey(0x1C);
						Sleep(500);
						lastPressTime = std::chrono::steady_clock::now();
					}
				}
			}
		}
		// You are technically always AFK until the keyboard thread says otherwise
		isafk = true;
	}
	
	// Automatically turn off these 4 modules if you leave roblox window (so it isn't annoying)
	if (!tabbedintoroblox) {
		isbhoploop = false;
		iswallwalkloop = false;
		isitemloop = false;
		isdesyncloop = false;
		isspeed = false;
	}

	std::this_thread::sleep_for(std::chrono::microseconds(50)); // Delay between main code loop (so your cpu doesn't die instantly)

}

	// Save Window Positions and Size before closing
	RECT windowrect;
	GetWindowRect(hwnd, &windowrect);

	if (windowrect.left < 0) {
		WindowPosX = 0;
	} else {
		WindowPosX = windowrect.left;
	}

	if (windowrect.top < 0) {
		WindowPosY = 0;
	} else {
		WindowPosY = windowrect.top;
	}

	RECT screen_rect;

	GetWindowRect(hwnd, &screen_rect);

	screen_width = screen_rect.right - screen_rect.left;
	screen_height = screen_rect.bottom - screen_rect.top;

	// If save file, save normally, if not, save as Profile 1
	if (!G_CURRENTLY_LOADED_PROFILE_NAME.empty()) {
		SaveSettings(G_SETTINGS_FILEPATH, G_CURRENTLY_LOADED_PROFILE_NAME);
	} else {
		std::ifstream file(G_SETTINGS_FILEPATH);
		if (!file.is_open()) {
			G_CURRENTLY_LOADED_PROFILE_NAME = "Profile 1";
			SaveSettings(G_SETTINGS_FILEPATH, G_CURRENTLY_LOADED_PROFILE_NAME);
		}
	}

	if (g_keyboardHook) {
		UnhookWindowsHookEx(g_keyboardHook);
		g_keyboardHook = NULL;
	}


	guiThread.join();

	ShutdownLinuxCompatLayer();

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	CleanupDeviceD3D();
	DestroyWindow(hwnd);
	timeEndPeriod(1);
	exit(0);

	return 0;
}