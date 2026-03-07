#include "Resource Files/wine_compatibility_layer.h"
#include "shared_mem.h"
#include "resource.h"
#include <Windows.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <TlHelp32.h>

#include "Resource Files/globals.h"
using namespace Globals;

std::string g_linuxHelperPath_Windows;

// Function Pointers
const HMODULE hNtdll = GetModuleHandleA("ntdll");
auto pfnSuspend = (LONG (*)(HANDLE))GetProcAddress(hNtdll, "NtSuspendProcess");
auto pfnResume = (LONG (*)(HANDLE))GetProcAddress(hNtdll, "NtResumeProcess");
typedef LONG(WINAPI *PntSuspendProcess)(HANDLE);
typedef LONG(WINAPI *PntResumeProcess)(HANDLE);

static SharedMemory *g_sharedData = nullptr;
static HANDLE g_hMapFile = NULL;

const char *LINUX_HELPER_BINARY_NAME = "Suspend_Input_Helper_Linux_Binary";
const char *LINUX_SHARED_MEM_FILE_WINE_PATH = "Z:\\tmp\\cross_input_shm_file";

// Internal Helpers (Keep these static or in anonymous namespace)
bool IsRunningOnWine()
{
	HKEY hKey;
	if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Wine", 0, KEY_READ, &hKey) ==
	    ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return true;
	}
	return false;
}

std::string GetWineHostOS()
{
	HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
	if (!hNtdll)
		return "unknown";
	typedef const char *(*wine_get_host_version_t)(const char **sysname, const char **release);
	auto wine_get_host_version = (wine_get_host_version_t)GetProcAddress(hNtdll, "wine_get_host_version");
	if (wine_get_host_version) {
		const char *sysname_ptr;
		wine_get_host_version(&sysname_ptr, nullptr);
		if (sysname_ptr) {
			std::string sysname_str(sysname_ptr);
			for (char &c : sysname_str) {
				c = tolower(c);
			}
			return sysname_str;
		}
	}
	return "unknown";
}

std::string ExecuteAndGetStdout(const std::string& cmd) {
    HANDLE hChildStd_OUT_Rd = NULL;
    HANDLE hChildStd_OUT_Wr = NULL;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &sa, 0)) {
        return "";
    }
    if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
        return "";
    }

    PROCESS_INFORMATION piProcInfo = {0};
    STARTUPINFOA siStartInfo = {0};
    siStartInfo.cb = sizeof(STARTUPINFOA);
    siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    siStartInfo.hStdOutput = hChildStd_OUT_Wr;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    siStartInfo.wShowWindow = SW_HIDE;

    std::vector<char> cmdVector(cmd.begin(), cmd.end());
    cmdVector.push_back('\0');

    if (!CreateProcessA(NULL, cmdVector.data(), NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo)) {
        CloseHandle(hChildStd_OUT_Wr);
        CloseHandle(hChildStd_OUT_Rd);
        return "";
    }
    
    CloseHandle(hChildStd_OUT_Wr); // Close the write end of the pipe in the parent

    std::string output = "";
    DWORD dwRead;
    CHAR chBuf[4096];
    while (ReadFile(hChildStd_OUT_Rd, chBuf, sizeof(chBuf) - 1, &dwRead, NULL) && dwRead != 0) {
        chBuf[dwRead] = '\0';
        output.append(chBuf);
    }

    CloseHandle(hChildStd_OUT_Rd);
    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);

    // Trim trailing newline characters from the output
    size_t lastChar = output.find_last_not_of(" \n\r\t");
    if (std::string::npos != lastChar) {
        output.erase(lastChar + 1);
    }
    return output;
}

// Main Logic for Launching
void InitLinuxCompatLayer() {
    if (!IsRunningOnWine() || GetWineHostOS() != "linux") {
        g_isLinuxWine = false;
        return;
    }

    // Enable VSync in wine to hopefully reduce CPU usage (Does nothing sadly...)
    (void)_putenv("vblank=0");
    std::cout << "Wine on Linux detected. Initializing helper process..." << std::endl;

    std::string helperWindowsPath; // Will be determined later

    // First, check if the shared memory file already exists. If it does, the helper is probably running.
    HANDLE hFileCheck = CreateFileA(LINUX_SHARED_MEM_FILE_WINE_PATH, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFileCheck != INVALID_HANDLE_VALUE) {
        CloseHandle(hFileCheck);
        std::cout << "Helper shared memory file found. Assuming helper is already running and proceeding to connect." << std::endl;
    } else {
        // If the file doesn't exist, we must extract and launch the helper.
        std::cout << "Helper not detected. Proceeding with extraction and launch..." << std::endl;

        // Find and Load the Embedded Binary Linux Helper within the exe
        HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(IDR_BINARY1), TEXT("Binary"));
        if (!hRes) {
            std::cerr << "Error: Could not find embedded resource IDR_BINARY1." << std::endl;
            g_isLinuxWine = false;
            return;
        }
        HGLOBAL hResLoad = LoadResource(NULL, hRes);
        void* pResData = LockResource(hResLoad);
        DWORD dwSize = SizeofResource(NULL, hRes);
        if (!pResData || dwSize == 0) {
            std::cerr << "Error: Could not load or lock embedded Linux helper binary. Resource might be empty." << std::endl;
            g_isLinuxWine = false;
            return;
        }

        // Write the Binary to a Temporary File with a Fixed Name
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        helperWindowsPath = std::string(tempPath) + LINUX_HELPER_BINARY_NAME;

        std::ofstream outFile(helperWindowsPath, std::ios::binary | std::ios::trunc);
        if (!outFile) {
            std::cerr << "Error: Failed to create temporary file for Linux helper at: " << helperWindowsPath << std::endl;
            g_isLinuxWine = false;
            return;
        }
        outFile.write(static_cast<const char*>(pResData), dwSize);
        outFile.close();
        std::cout << "Helper binary written to: " << helperWindowsPath << std::endl;

        // Translate the Windows Path to a Linux Path using winepath
        std::string winepathCmd = "winepath.exe -u \"" + helperWindowsPath + "\"";
        std::string helperLinuxPath = ExecuteAndGetStdout(winepathCmd);
        
        // Trim trailing whitespace/newlines from command output
        size_t lastChar = helperLinuxPath.find_last_not_of(" \n\r\t");
        if (std::string::npos != lastChar) {
            helperLinuxPath.erase(lastChar + 1);
        }

        if (helperLinuxPath.empty()) {
            std::cerr << "Error: 'winepath' failed to translate the helper path. Ensure wine-binfmt is configured." << std::endl;
            remove(helperWindowsPath.c_str());
            g_isLinuxWine = false;
            return;
        }
        std::cout << "Helper's Linux path: " << helperLinuxPath << std::endl;

        // Make the File Executable using its Linux Path
        std::string chmodCommand = "start /unix /bin/sh -c \"chmod +x '" + helperLinuxPath + "'\"";
        STARTUPINFOA si_chmod = {0};
        PROCESS_INFORMATION pi_chmod = {0};
        si_chmod.cb = sizeof(si_chmod);
        si_chmod.dwFlags = STARTF_USESHOWWINDOW;
        si_chmod.wShowWindow = SW_HIDE;
        std::vector<char> chmodCmdVector(chmodCommand.begin(), chmodCommand.end());
        chmodCmdVector.push_back('\0');

        if (!CreateProcessA(NULL, chmodCmdVector.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si_chmod, &pi_chmod)) {
            std::cerr << "Error: CreateProcess failed for chmod command." << std::endl;
            remove(helperWindowsPath.c_str());
            g_isLinuxWine = false;
            return;
        }

        WaitForSingleObject(pi_chmod.hProcess, INFINITE); // Wait for chmod to finish
        CloseHandle(pi_chmod.hProcess);
        CloseHandle(pi_chmod.hThread);

        char currentExePath[MAX_PATH];
        GetModuleFileNameA(NULL, currentExePath, MAX_PATH);
        std::string currentExeName = currentExePath;
        // Extract just the filename without path
        size_t lastSlash = currentExeName.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            currentExeName = currentExeName.substr(lastSlash + 1);
        }

        // Launch the Helper using a Graphical Sudo Wrapper
        std::string sudoCommand = 
            "start /unix /bin/sh -c \""
            "if command -v zenity >/dev/null; then "
                "zenity --password --title='Authentication Required' --text='Enter your password to run the Input Helper:' | sudo -S -p '' '" + helperLinuxPath + "' '" + currentExeName + "';"  // Added argument here
            "elif command -v kdialog >/dev/null; then "
                "kdialog --password 'Enter your password to run the Input Helper:' | sudo -S -p '' '" + helperLinuxPath + "' '" + currentExeName + "';"  // And here
            "fi"
            "\"";

        STARTUPINFOA si_exec = {0};
        PROCESS_INFORMATION pi_exec = {0};
        si_exec.cb = sizeof(si_exec);
        si_exec.dwFlags = STARTF_USESHOWWINDOW;
        si_exec.wShowWindow = SW_HIDE;
        std::vector<char> execCmdVector(sudoCommand.begin(), sudoCommand.end());
        execCmdVector.push_back('\0');

        if (!CreateProcessA(NULL, execCmdVector.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si_exec, &pi_exec)) {
            std::cerr << "Error: CreateProcess failed to launch the graphical sudo command." << std::endl;
            remove(helperWindowsPath.c_str());
            g_isLinuxWine = false;
            return;
        }


        CloseHandle(pi_exec.hProcess);
        CloseHandle(pi_exec.hThread);

        // Step 6: Wait for the Helper's Shared Memory File to Appear
        bool helperReady = false;
        std::cout << "Waiting for helper to become ready (up to 60 seconds)..." << std::endl;

        for (int i = 0; i < 20; ++i) {
            HANDLE hPoll = CreateFileA(LINUX_SHARED_MEM_FILE_WINE_PATH, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hPoll != INVALID_HANDLE_VALUE) {
                CloseHandle(hPoll);
                helperReady = true;
                std::cout << "\nHelper is ready. Shared memory file detected." << std::endl;
                break;
            }
            
            // Add detailed logging on the first failure
            if (i == 0) {
                DWORD lastError = GetLastError();
                if (lastError == ERROR_FILE_NOT_FOUND) {
                    std::cout << "Log: Waiting for file '" << LINUX_SHARED_MEM_FILE_WINE_PATH << "' to be created..." << std::flush;
                } else if (lastError == ERROR_ACCESS_DENIED) {
                    std::cerr << "\nWarning: Access was denied while checking for the shared memory file. This could indicate a permissions problem with the '/tmp' directory on your Linux system." << std::endl;
                } else {
                    std::cerr << "\nWarning: An unexpected error occurred while checking for the helper file. Win32 Error Code: " << lastError << std::endl;
                }
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "." << std::flush;
        }

        if (!helperReady) {
            std::cerr << "\nError: Timed out waiting for the helper process to create the shared memory file." << std::endl;
            remove(helperWindowsPath.c_str()); // Clean up the helper binary on failure
            g_isLinuxWine = false;
            MessageBoxW(
                NULL,
                L"The macro cannot find the linux helper loaded in ram after 20 seconds. Did you incorrectly answer the sudo prompt? Did you run it through non-standard wine?",
                L"Error",
                MB_ICONERROR | MB_OK
            );
	        std::exit(1);
            return;
        }
    }

    // === Step 7: Map the Shared Memory Now That We Know It Exists ===
    std::cout << "Connecting to shared memory at: " << LINUX_SHARED_MEM_FILE_WINE_PATH << std::endl;
    HANDLE hFile = CreateFileA(LINUX_SHARED_MEM_FILE_WINE_PATH, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Error: CreateFileA failed to open the helper's memory map file even after it was detected. Error: " << GetLastError() << std::endl;
        if (!helperWindowsPath.empty()) remove(helperWindowsPath.c_str());
        g_isLinuxWine = false;
        return;
    }

    g_hMapFile = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, sizeof(SharedMemory), NULL);
    CloseHandle(hFile);
    if (g_hMapFile == NULL) {
        std::cerr << "Error: CreateFileMappingA failed. Error: " << GetLastError() << std::endl;
        if (!helperWindowsPath.empty()) remove(helperWindowsPath.c_str());
        g_isLinuxWine = false;
        return;
    }

    g_sharedData = (SharedMemory*)MapViewOfFile(g_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedMemory));
    if (g_sharedData == nullptr) {
        std::cerr << "Error: MapViewOfFile failed. Error: " << GetLastError() << std::endl;
        CloseHandle(g_hMapFile);
        if (!helperWindowsPath.empty()) remove(helperWindowsPath.c_str());
        g_isLinuxWine = false;
        return;
    }

    // Final Success
    g_isLinuxWine = true;
    std::cout << "Initialization successful. Shared memory is now mapped." << std::endl;
}

void ShutdownLinuxCompatLayer() {
    if (g_isLinuxWine) {
        if (g_sharedData) UnmapViewOfFile(g_sharedData);
        if (g_hMapFile) CloseHandle(g_hMapFile);
    }
}

// Internal function to write a command to the circular buffer
void EnqueueCommand(const Command& new_cmd) {
    if (!g_isLinuxWine || !g_sharedData) return;

    uint32_t write_idx = g_sharedData->write_index.load(std::memory_order_relaxed);
    uint32_t next_write_idx = (write_idx + 1) & (COMMAND_BUFFER_SIZE - 1);
    
    // Check if buffer is full
    if (next_write_idx == g_sharedData->read_index.load(std::memory_order_acquire)) {
	    std::cout << "Linux Binary Helper Queue Limit is full!!! Dropping input.";
        return; // Drop input
    }
    
    // Get a reference to the command slot we are about to write to
    auto& cmd_slot = g_sharedData->command_buffer[write_idx];

    // We must now copy ALL possible data fields from the temporary command
    // object into the shared memory slot. The missing line was for target_pid.

    // Copy input-related data
    cmd_slot.win_vk_code.store(new_cmd.win_vk_code.load(std::memory_order_relaxed), std::memory_order_relaxed);
    cmd_slot.value.store(new_cmd.value.load(std::memory_order_relaxed), std::memory_order_relaxed);
    cmd_slot.rel_x.store(new_cmd.rel_x.load(std::memory_order_relaxed), std::memory_order_relaxed);
    cmd_slot.rel_y.store(new_cmd.rel_y.load(std::memory_order_relaxed), std::memory_order_relaxed);
    
    // Store PID's we want to use
    cmd_slot.target_pid.store(new_cmd.target_pid.load(std::memory_order_relaxed), std::memory_order_relaxed);
    
    // The final store on type makes the entire command visible to the reader atomically.
    cmd_slot.type.store(new_cmd.type.load(std::memory_order_relaxed), std::memory_order_release);

    // Finally, update the write index to publish the new command
    g_sharedData->write_index.store(next_write_idx, std::memory_order_release);
}

// API Replacement Functions

// Replacement for GetAsyncKeyState
bool IsKeyPressed(WORD vk_key)
{
	if (g_isLinuxWine) {
		if (vk_key > 255)
			return false;
		return g_sharedData->key_states[vk_key].load(std::memory_order_acquire);
	}

	if (vk_key == VK_MOUSE_WHEEL_UP) {
		return IsWheelUp();
    }

	if (vk_key == VK_MOUSE_WHEEL_DOWN) {
		return IsWheelDown();
	}
	return ::GetAsyncKeyState(vk_key) & 0x8000;
}

void HoldKey(WORD scanCode)
{
	if (g_isLinuxWine) {
		// Convert the scan code to a virtual-key code, which our system uses.
		WORD vkCode = MapVirtualKeyA(scanCode, MAPVK_VSC_TO_VK);
		if (vkCode == 0)
			return; // Conversion failed

		Command cmd = {};
		cmd.type.store(CMD_KEY_ACTION, std::memory_order_relaxed);
		cmd.win_vk_code.store(vkCode, std::memory_order_relaxed);
		cmd.value.store(1, std::memory_order_relaxed); // 1 = press
		EnqueueCommand(cmd);
	} else {
		// Original Windows method
		INPUT input = {};
		input.type = INPUT_KEYBOARD;
		input.ki.wScan = scanCode;
		input.ki.dwFlags = KEYEVENTF_SCANCODE;
		SendInput(1, &input, sizeof(INPUT));
	}
}

void HoldKey(WORD scanCode, bool extended)
{
    if (g_isLinuxWine) {
        // Linux/Wine doesn't need extended flag, use the original function
        HoldKey(scanCode);
    } else {
        // Windows method with extended flag
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wScan = scanCode;
        input.ki.dwFlags = KEYEVENTF_SCANCODE;
        if (extended) {
            input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        }
        SendInput(1, &input, sizeof(INPUT));
    }
}

void ReleaseKey(WORD scanCode)
{
	if (g_isLinuxWine) {
		// Convert the scan code to a virtual-key code.
		WORD vkCode = MapVirtualKeyA(scanCode, MAPVK_VSC_TO_VK);
		if (vkCode == 0)
			return; // Conversion failed

		Command cmd = {};
		cmd.type.store(CMD_KEY_ACTION, std::memory_order_relaxed);
		cmd.win_vk_code.store(vkCode, std::memory_order_relaxed);
		cmd.value.store(0, std::memory_order_relaxed); // 0 = release
		EnqueueCommand(cmd);
	} else {
		// Original Windows method
		INPUT input = {};
		input.type = INPUT_KEYBOARD;
		input.ki.wScan = scanCode;
		input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
		SendInput(1, &input, sizeof(INPUT));
	}
}

void ReleaseKey(WORD scanCode, bool extended)
{
    if (g_isLinuxWine) {
        // Linux/Wine doesn't need extended flag - use the original function
        ReleaseKey(scanCode);
    } else {
        // Windows method with extended flag
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wScan = scanCode;
        input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
        if (extended) {
            input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        }
        SendInput(1, &input, sizeof(INPUT));
    }
}

void SendLinuxMouseWheel(int delta) {
    Command cmd = {};
    cmd.type.store(CMD_MOUSE_WHEEL, std::memory_order_relaxed);
    cmd.value.store(delta, std::memory_order_relaxed);  // + or - delta value
    EnqueueCommand(cmd);
}

void SetLinuxBhopState(bool enable) {
    Command cmd = {};
    if (enable) {
        cmd.type.store(CMD_BHOP_ENABLE, std::memory_order_relaxed);
    } else {
        cmd.type.store(CMD_BHOP_DISABLE, std::memory_order_relaxed);
    }
    EnqueueCommand(cmd);
}

void SetDesyncState(bool enable) {
    Command cmd = {};
    if (enable) {
        cmd.type.store(CMD_DESYNC_ENABLE, std::memory_order_relaxed);
    } else {
        cmd.type.store(CMD_DESYNC_DISABLE, std::memory_order_relaxed);
    }
    EnqueueCommand(cmd);
}

void HoldKeyBinded(unsigned int combinedKey) {
    // 1. Extract Flags and Key
    WORD vk = combinedKey & 0xFFFF; 
    bool useWin   = (combinedKey & 0x80000) != 0; // HOTKEY_MASK_WIN
    bool useCtrl  = (combinedKey & 0x20000) != 0; 
    bool useAlt   = (combinedKey & 0x40000) != 0; 
    bool useShift = (combinedKey & 0x10000) != 0; 

    if (g_isLinuxWine) {
        if (vk == VK_MOUSE_WHEEL_UP) {
            SendLinuxMouseWheel(WHEEL_DELTA * 100);
            return;
        } else if (vk == VK_MOUSE_WHEEL_DOWN) {
            SendLinuxMouseWheel(-WHEEL_DELTA * 100);
            return;
        }
        
        // Press Modifiers (Win -> Ctrl -> Alt -> Shift)
        if (useWin)   { Command c={}; c.type.store(CMD_KEY_ACTION); c.win_vk_code.store(VK_LWIN);    c.value.store(1); EnqueueCommand(c); }
        if (useCtrl)  { Command c={}; c.type.store(CMD_KEY_ACTION); c.win_vk_code.store(VK_CONTROL); c.value.store(1); EnqueueCommand(c); }
        if (useAlt)   { Command c={}; c.type.store(CMD_KEY_ACTION); c.win_vk_code.store(VK_MENU);    c.value.store(1); EnqueueCommand(c); }
        if (useShift) { Command c={}; c.type.store(CMD_KEY_ACTION); c.win_vk_code.store(VK_SHIFT);   c.value.store(1); EnqueueCommand(c); }

        // Press Main Key
        Command cmd = {};
        cmd.type.store(CMD_KEY_ACTION, std::memory_order_relaxed);
        cmd.win_vk_code.store(vk, std::memory_order_relaxed);
        cmd.value.store(1, std::memory_order_relaxed);
        EnqueueCommand(cmd);

    } else {
        std::vector<INPUT> inputs;
        
        // 1. Modifiers Down
        if (useWin)   { INPUT i={}; i.type=INPUT_KEYBOARD; i.ki.wVk=VK_LWIN;    inputs.push_back(i); }
        if (useCtrl)  { INPUT i={}; i.type=INPUT_KEYBOARD; i.ki.wVk=VK_CONTROL; inputs.push_back(i); }
        if (useAlt)   { INPUT i={}; i.type=INPUT_KEYBOARD; i.ki.wVk=VK_MENU;    inputs.push_back(i); }
        if (useShift) { INPUT i={}; i.type=INPUT_KEYBOARD; i.ki.wVk=VK_SHIFT;   inputs.push_back(i); }

        // 2. Main Key/Mouse Down
        INPUT mainInput = {};
        
        if (vk == VK_MOUSE_WHEEL_UP) {
            mainInput.type = INPUT_MOUSE;
            mainInput.mi.dwFlags = MOUSEEVENTF_WHEEL;
            mainInput.mi.mouseData = WHEEL_DELTA * 100;
        }
        else if (vk == VK_MOUSE_WHEEL_DOWN) {
            mainInput.type = INPUT_MOUSE;
            mainInput.mi.dwFlags = MOUSEEVENTF_WHEEL;
            mainInput.mi.mouseData = -WHEEL_DELTA * 100;
        }
        else if (vk >= VK_LBUTTON && vk <= VK_XBUTTON2) {
            mainInput.type = INPUT_MOUSE;
            if (vk == VK_LBUTTON) mainInput.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            else if (vk == VK_RBUTTON) mainInput.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
            else if (vk == VK_MBUTTON) mainInput.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
            else if (vk == VK_XBUTTON1) { mainInput.mi.dwFlags = MOUSEEVENTF_XDOWN; mainInput.mi.mouseData = XBUTTON1; }
            else if (vk == VK_XBUTTON2) { mainInput.mi.dwFlags = MOUSEEVENTF_XDOWN; mainInput.mi.mouseData = XBUTTON2; }
        } else {
            mainInput.type = INPUT_KEYBOARD;
            mainInput.ki.wScan = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
            mainInput.ki.dwFlags = KEYEVENTF_SCANCODE;
        }
        inputs.push_back(mainInput);

        SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
    }
}

void ReleaseKeyBinded(unsigned int combinedKey) {
    WORD vk = combinedKey & 0xFFFF;
    bool useWin   = (combinedKey & 0x80000) != 0;
    bool useCtrl  = (combinedKey & 0x20000) != 0;
    bool useAlt   = (combinedKey & 0x40000) != 0;
    bool useShift = (combinedKey & 0x10000) != 0;

    if (g_isLinuxWine) {
        // Release Main Key
        Command cmd = {};
        cmd.type.store(CMD_KEY_ACTION, std::memory_order_relaxed);
        cmd.win_vk_code.store(vk, std::memory_order_relaxed);
        cmd.value.store(0, std::memory_order_relaxed);
        EnqueueCommand(cmd);

        // Release Modifiers (Reverse Order: Shift -> Alt -> Ctrl -> Win)
        if (useShift) { Command c={}; c.type.store(CMD_KEY_ACTION); c.win_vk_code.store(VK_SHIFT);   c.value.store(0); EnqueueCommand(c); }
        if (useAlt)   { Command c={}; c.type.store(CMD_KEY_ACTION); c.win_vk_code.store(VK_MENU);    c.value.store(0); EnqueueCommand(c); }
        if (useCtrl)  { Command c={}; c.type.store(CMD_KEY_ACTION); c.win_vk_code.store(VK_CONTROL); c.value.store(0); EnqueueCommand(c); }
        if (useWin)   { Command c={}; c.type.store(CMD_KEY_ACTION); c.win_vk_code.store(VK_LWIN);    c.value.store(0); EnqueueCommand(c); }

    } else {
        std::vector<INPUT> inputs;

        // 1. Main Key/Mouse Up
        INPUT mainInput = {};
        if (vk >= VK_LBUTTON && vk <= VK_XBUTTON2) {
            mainInput.type = INPUT_MOUSE;
            if (vk == VK_LBUTTON) mainInput.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            else if (vk == VK_RBUTTON) mainInput.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
            else if (vk == VK_MBUTTON) mainInput.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
            else if (vk == VK_XBUTTON1) { mainInput.mi.dwFlags = MOUSEEVENTF_XUP; mainInput.mi.mouseData = XBUTTON1; }
            else if (vk == VK_XBUTTON2) { mainInput.mi.dwFlags = MOUSEEVENTF_XUP; mainInput.mi.mouseData = XBUTTON2; }
        } else {
            mainInput.type = INPUT_KEYBOARD;
            mainInput.ki.wScan = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
            mainInput.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
        }
        inputs.push_back(mainInput);

        // 2. Release Modifiers (Reverse Order)
        if (useShift) { INPUT i={}; i.type=INPUT_KEYBOARD; i.ki.wVk=VK_SHIFT;   i.ki.dwFlags=KEYEVENTF_KEYUP; inputs.push_back(i); }
        if (useAlt)   { INPUT i={}; i.type=INPUT_KEYBOARD; i.ki.wVk=VK_MENU;    i.ki.dwFlags=KEYEVENTF_KEYUP; inputs.push_back(i); }
        if (useCtrl)  { INPUT i={}; i.type=INPUT_KEYBOARD; i.ki.wVk=VK_CONTROL; i.ki.dwFlags=KEYEVENTF_KEYUP; inputs.push_back(i); }
        if (useWin)   { INPUT i={}; i.type=INPUT_KEYBOARD; i.ki.wVk=VK_LWIN;    i.ki.dwFlags=KEYEVENTF_KEYUP; inputs.push_back(i); }

        SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
    }
}

void MoveMouse(int dx, int dy)
{
	if (g_isLinuxWine) {
		Command cmd = {};
		cmd.type.store(CMD_MOUSE_MOVE_RELATIVE, std::memory_order_relaxed);
		cmd.rel_x.store(dx, std::memory_order_relaxed);
		cmd.rel_y.store(dy, std::memory_order_relaxed);
		EnqueueCommand(cmd);
	} else {
		INPUT input = {0};
		input.type = INPUT_MOUSE;
        // Scale dx and dy with display scale
		dx = static_cast<int>(static_cast<int64_t>(dx) * display_scale / 100);
		dy = static_cast<int>(static_cast<int64_t>(dy) * display_scale / 100);
		input.mi.dx = dx;
		input.mi.dy = dy;
		input.mi.dwFlags = MOUSEEVENTF_MOVE;
		SendInput(1, &input, sizeof(INPUT));
	}
}


// Special Action script to send a command to linux to freeze a PID
bool Linux_ExecuteSpecialAction(SpecialAction& action_data, int timeout_ms = 1000) {
    if (!g_isLinuxWine || !g_sharedData) return false;
    
    // Wait for the mailbox to be free
    auto start_wait = std::chrono::steady_clock::now();
    while (g_sharedData->special_action.request_ready.load() || g_sharedData->special_action.response_ready.load()) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_wait).count() > timeout_ms) return false;
        Sleep(1);
    }
    
    // Place the command in the mailbox
    strcpy_s(g_sharedData->special_action.process_name, sizeof(g_sharedData->special_action.process_name), action_data.process_name);
    g_sharedData->special_action.command.store(action_data.command.load(), std::memory_order_relaxed);
    g_sharedData->special_action.request_ready.store(true, std::memory_order_release);
    
    // Wait for the response
    auto start_response = std::chrono::steady_clock::now();
    while (!g_sharedData->special_action.response_ready.load(std::memory_order_acquire)) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_response).count() > timeout_ms) {
            g_sharedData->special_action.request_ready.store(false);
            return false;
        }
        Sleep(1);
    }
    
    // Retrieve the response
    bool success = g_sharedData->special_action.response_success.load();
    if (success) {
        // Copy the PID count and the array of PIDs back into our local action_data struct.
        int count = g_sharedData->special_action.response_pid_count.load();
        action_data.response_pid_count.store(count);
        for (int i = 0; i < count; ++i) {
            action_data.response_pids[i] = g_sharedData->special_action.response_pids[i];
        }
    }
    
    g_sharedData->special_action.response_ready.store(false, std::memory_order_relaxed);
    return success;
}

void SetBhopDelay(int delay_ms) {
    SpecialAction action = {};
    action.command.store(SA_SET_BHOP_DELAY);
    action.response_success.store(false);
    action.response_pid_count.store(0);
    strcpy_s(action.process_name, sizeof(action.process_name), ""); // No process name needed

    snprintf(action.process_name, sizeof(action.process_name), "%d", delay_ms);

    Linux_ExecuteSpecialAction(action);
}

void SetDesyncItem(int itemSlot) {
    SpecialAction action = {};
    action.command.store(SA_SET_DESYNC_ITEM);
    action.response_success.store(false);
    action.response_pid_count.store(0);
    strcpy_s(action.process_name, sizeof(action.process_name), ""); // No process name needed

    snprintf(action.process_name, sizeof(action.process_name), "%d", itemSlot);

    Linux_ExecuteSpecialAction(action);
}


void SuspendOrResumeProcesses_Compat(const std::vector<DWORD>& pids, const std::vector<HANDLE>& handles, bool suspend) {
    if (g_isLinuxWine) {
        // On Linux, we ignore the (empty) handles vector and use the PIDs.
        for (DWORD pid : pids) {
            Command cmd = {};
            cmd.type.store(suspend ? CMD_SUSPEND_PROCESS : CMD_RESUME_PROCESS);
            cmd.target_pid.store(pid);
            EnqueueCommand(cmd);
        }
    } 
    else {
        // On Windows, we ignore the PIDs and use the pre-opened handles for max performance.
        if (handles.empty()) return;
        
        if (suspend) {
            if (!pfnSuspend) return;
            for (HANDLE hProcess : handles) {
                pfnSuspend(hProcess);
            }
        } else {
            if (!pfnResume) return;
            for (HANDLE hProcess : handles) {
                pfnResume(hProcess);
            }
        }
    }
}

void SetLagswitchEnabled(bool enable) {
    if (g_isLinuxWine) {
        Command cmd = {};
        cmd.type.store(enable ? CMD_LAGSWITCH_ENABLE : CMD_LAGSWITCH_DISABLE, std::memory_order_relaxed);
        EnqueueCommand(cmd);
    } else {
        // Windows: directly set the blocking state
        g_windivert_blocking = enable;
    }
}

void SetLagswitchIPs(const std::vector<std::string>& ips) {
    if (g_isLinuxWine) {
        SpecialAction action = {};
        action.command.store(SA_SET_LAGSWITCH_IPS, std::memory_order_relaxed);
        action.response_success.store(false);
        action.response_pid_count.store(0);
        
        // Serialize IPs into process_name field (comma-separated)
        std::string ip_list;
        for (size_t i = 0; i < ips.size(); ++i) {
            ip_list += ips[i];
            if (i < ips.size() - 1) ip_list += ",";
        }
        strcpy_s(action.process_name, sizeof(action.process_name), ip_list.c_str());
        
        Linux_ExecuteSpecialAction(action);
    }
    // Windows: IP filtering is handled by WinDivert filter in network_manager.cpp
}

void SetLagswitchDirection(bool inbound, bool outbound) {
    if (g_isLinuxWine) {
        Command cmd = {};
        cmd.type.store(CMD_LAGSWITCH_SET_DIRECTION, std::memory_order_relaxed);
        cmd.value.store((inbound ? 1 : 0) | ((outbound ? 1 : 0) << 1), std::memory_order_relaxed);
        EnqueueCommand(cmd);
    }
    // Windows: direction filtering is handled by WinDivert filter in network_manager.cpp
}

void SetLagswitchDelay(int delay_ms) {
    if (g_isLinuxWine) {
        Command cmd = {};
        cmd.type.store(CMD_LAGSWITCH_SET_DELAY, std::memory_order_relaxed);
        cmd.value.store(delay_ms, std::memory_order_relaxed);
        EnqueueCommand(cmd);
    }
    // Windows: delay is handled by the delay queue in network_manager.cpp
}

void SetLagswitchMode(int mode) {
    if (g_isLinuxWine) {
        Command cmd = {};
        cmd.type.store(CMD_LAGSWITCH_SET_MODE, std::memory_order_relaxed);
        cmd.value.store(mode, std::memory_order_relaxed);
        EnqueueCommand(cmd);
    }
    // Windows: mode (block vs delay) is handled by WinDivert logic in network_manager.cpp
}
