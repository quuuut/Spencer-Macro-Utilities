#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <signal.h>
#include <ctime>
#include <cstring>
#include <thread>
#include <chrono>
#include <csignal>
#include <climits>
#include <cstdio>
#include <pwd.h>
#include <cerrno>
#include <cstdlib>
#include <stdexcept>
#include <libproc.h>

// macOS-specific headers
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>

#include "shared_mem.h"
#include "keymapping.h"

// --- Globals ---
SharedMemory* shared_data = nullptr;
CGEventSourceRef event_source = nullptr;
const char* MM_FILE_PATH = "/tmp/cross_input_shm_file";
bool isbhop = false;
bool isdesync = false;

int bhop_delay = 10;
int desync_itemslot = 5;

// --- Function Prototypes ---
void cleanup(int);
int setup_event_source();
void emit_key_event(uint16_t macos_keycode, bool key_down);
void emit_mouse_button_event(CGMouseButton button, CGEventType down_type, CGEventType up_type, bool is_down);
void emit_mouse_move_event(int32_t dx, int32_t dy);
void emit_mouse_wheel_event(int32_t delta);
void command_processor_thread();
void special_action_thread();
void fastkey_thread();
void event_tap_thread();

pid_t find_main_process_by_name(const std::string& name);
std::vector<pid_t> find_all_processes_by_exes_or_pids(const std::string& name);
void monitor_parent_process(const std::string& parent_process_name);

// ********************************************************************
// *                  CORE HELPER FUNCTIONS                           *
// ********************************************************************

// Get parent PID on macOS
pid_t get_ppid(pid_t pid) {
    struct proc_bsdinfo proc_info;
    if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &proc_info, sizeof(proc_info)) <= 0) {
        return -1;
    }
    return proc_info.pbi_ppid;
}

// Process suspension on macOS (no cgroups, just signals)
void hybrid_suspend_or_resume(pid_t pid, bool suspend) {
    const char* action = suspend ? "SIGSTOP" : "SIGCONT";
    int signal_to_send = suspend ? SIGSTOP : SIGCONT;
    
    printf("[Helper] Sending %s to PID %d\n", action, pid);
    if (kill(pid, signal_to_send) != 0) {
        perror(("[Helper] Error sending " + std::string(action)).c_str());
    }
}

void monitor_parent_process(const std::string& parent_process_name) {
    std::cout << "[Monitor] Parent process monitor thread started for '" << parent_process_name << "'." << std::endl;
    
    while (true) {
        // Use pgrep to check if process exists
        std::string command = "pgrep -x \"" + parent_process_name + "\" > /dev/null 2>&1";
        int result = system(command.c_str());
        
        if (result != 0) {
            std::cout << "[Monitor] Parent process '" << parent_process_name << "' not found. Initiating self-shutdown." << std::endl;
            raise(SIGINT);
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "[Monitor] Monitor thread exiting." << std::endl;
}

// ********************************************************************
// *                  MAIN APPLICATION LOGIC                          *
// ********************************************************************

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <filename>" << std::endl;
        std::cout << "Please provide exactly one filename as an argument." << std::endl;
        return 1;
    }

    std::string filename = argv[1];
    signal(SIGINT, cleanup);

    // Create the Shared Memory File
    std::cout << "[Helper] Creating shared memory file..." << std::endl;
    int shm_fd = open(MM_FILE_PATH, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("[Helper] CRITICAL: open failed for shared memory file");
        return 1;
    }
    
    if (fchmod(shm_fd, 0666) == -1) {
        perror("[Helper] CRITICAL: fchmod failed on shared memory file");
        close(shm_fd);
        unlink(MM_FILE_PATH);
        return 1;
    }
    
    if (ftruncate(shm_fd, sizeof(SharedMemory)) == -1) {
        perror("[Helper] CRITICAL: ftruncate failed");
        close(shm_fd);
        unlink(MM_FILE_PATH);
        return 1;
    }

    shared_data = (SharedMemory*)mmap(0, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);

    if (shared_data == MAP_FAILED) {
        perror("[Helper] CRITICAL: mmap failed");
        unlink(MM_FILE_PATH);
        return 1;
    }

    memset(shared_data, 0, sizeof(SharedMemory));
    std::cout << "[Helper] Memory mapped file created successfully." << std::endl;
    
    std::thread monitor(monitor_parent_process, filename);
    monitor.detach();

    // Check for Accessibility Permissions
    std::cout << "[Helper] Checking accessibility permissions..." << std::endl;
    
    // Create CFDictionary for trusted check with prompt
    const void* keys[] = { kAXTrustedCheckOptionPrompt };
    const void* values[] = { kCFBooleanTrue };
    CFDictionaryRef options = CFDictionaryCreate(
        kCFAllocatorDefault,
        keys,
        values,
        1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );
    
    Boolean trusted = AXIsProcessTrustedWithOptions(options);
    CFRelease(options);
    
    if (!trusted) {
        std::cerr << "[Helper] ERROR: Accessibility permissions not granted!" << std::endl;
        std::cerr << "[Helper] Please grant accessibility permissions in System Preferences > Security & Privacy > Privacy > Accessibility" << std::endl;
        cleanup(0);
        return 1;
    }
    
    std::cout << "[Helper] Accessibility permissions granted." << std::endl;

    // Setup Event Source and Worker Threads
    std::cout << "[Helper] Starting up core services..." << std::endl;
    
    if (setup_event_source() != 0) {
        cleanup(0);
        return 1;
    }
    
    std::cout << "[Helper] Event source created." << std::endl;
    std::cout << "[Helper] Launching worker threads. Press Ctrl+C to exit." << std::endl;
    
    std::thread event_tap(event_tap_thread);
    std::thread processor(command_processor_thread);
    std::thread special_actions(special_action_thread);
    std::thread fastkey(fastkey_thread);

    event_tap.join();
    processor.join();
    special_actions.join();
    fastkey.join();
    
    cleanup(0);
    return 0;
}

void cleanup(int signum) {
    std::cout << "\nCleaning up..." << std::endl;
    
    if (event_source != nullptr) {
        CFRelease(event_source);
        event_source = nullptr;
    }
    
    if (shared_data != nullptr && shared_data != MAP_FAILED) {
        munmap(shared_data, sizeof(SharedMemory));
    }
    
    unlink(MM_FILE_PATH);
    exit(0);
}

int setup_event_source() {
    event_source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    if (event_source == nullptr) {
        std::cerr << "[Helper] Failed to create event source." << std::endl;
        return -1;
    }
    return 0;
}

// ********************************************************************
// *                  EVENT EMISSION FUNCTIONS                        *
// ********************************************************************

void emit_key_event(uint16_t macos_keycode, bool key_down) {
    CGEventRef event = CGEventCreateKeyboardEvent(event_source, macos_keycode, key_down);
    if (event) {
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
    }
}

void emit_mouse_button_event(CGMouseButton button, CGEventType down_type, CGEventType up_type, bool is_down) {
    CGEventType event_type = is_down ? down_type : up_type;
    CGEventRef temp_event = CGEventCreate(NULL);
    CGPoint current_pos = CGEventGetLocation(temp_event);
    CFRelease(temp_event);
    
    CGEventRef event = CGEventCreateMouseEvent(event_source, event_type, current_pos, button);
    if (event) {
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
    }
}

void emit_mouse_move_event(int32_t dx, int32_t dy) {
    CGEventRef temp_event = CGEventCreate(NULL);
    CGPoint current_pos = CGEventGetLocation(temp_event);
    CFRelease(temp_event);
    
    CGPoint new_pos = CGPointMake(current_pos.x + dx, current_pos.y + dy);
    
    CGEventRef event = CGEventCreateMouseEvent(event_source, kCGEventMouseMoved, new_pos, kCGMouseButtonLeft);
    if (event) {
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
    }
}

void emit_mouse_wheel_event(int32_t delta) {
    // macOS uses wheel count, positive = up, negative = down
    CGEventRef event = CGEventCreateScrollWheelEvent(event_source, kCGScrollEventUnitLine, 1, delta);
    if (event) {
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
    }
}

// ********************************************************************
// *                  EVENT TAP THREAD                                *
// ********************************************************************

CGEventRef event_tap_callback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* refcon) {
    if (type == kCGEventKeyDown || type == kCGEventKeyUp) {
        CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        uint8_t vk_code = macos_to_win_vkey(keycode);
        
        if (vk_code != VK_UNASSIGNED) {
            bool is_down = (type == kCGEventKeyDown);
            
            // Handle bhop special case
            if (isbhop && vk_code == 0x20) {
                // Don't update shared state for space when bhop is active
            } else {
                shared_data->key_states[vk_code].store(is_down, std::memory_order_release);
            }
        }
    }
    
    return event;
}

void event_tap_thread() {
    CGEventMask event_mask = CGEventMaskBit(kCGEventKeyDown) | 
                              CGEventMaskBit(kCGEventKeyUp) |
                              CGEventMaskBit(kCGEventLeftMouseDown) |
                              CGEventMaskBit(kCGEventLeftMouseUp) |
                              CGEventMaskBit(kCGEventRightMouseDown) |
                              CGEventMaskBit(kCGEventRightMouseUp);
    
    CFMachPortRef event_tap = CGEventTapCreate(
        kCGSessionEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionListenOnly,
        event_mask,
        event_tap_callback,
        nullptr
    );
    
    if (!event_tap) {
        std::cerr << "[Helper] Failed to create event tap. Make sure accessibility permissions are granted." << std::endl;
        return;
    }
    
    CFRunLoopSourceRef run_loop_source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, event_tap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), run_loop_source, kCFRunLoopCommonModes);
    CGEventTapEnable(event_tap, true);
    
    std::cout << "[Helper] Event tap created and enabled." << std::endl;
    
    CFRunLoopRun();
    
    CFRelease(run_loop_source);
    CFRelease(event_tap);
}

// ********************************************************************
// *                  WORKER THREADS                                  *
// ********************************************************************

void fastkey_thread() {
    while (isbhop) {
        bool space_pressed = shared_data->key_states[0x20].load(std::memory_order_acquire);
        if (space_pressed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(bhop_delay/2)); // Short delay

            emit_key_event(0x31, false); // 0x31 = space on macOS
            std::this_thread::sleep_for(std::chrono::milliseconds(bhop_delay/2)); // Short delay

            emit_key_event(0x31, true);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    while (isdesync) {
        std::this_thread::sleep_for(std::chrono::nanoseconds(100));
        // Map itemslot (1-9) to macOS keycodes (0x12-0x19 for 1-8, 0x19 for 9)
        uint16_t keycode = (desync_itemslot == 9) ? 0x19 : (0x12 + desync_itemslot - 1);
        emit_key_event(keycode, true);
        std::this_thread::sleep_for(std::chrono::nanoseconds(100));
        emit_key_event(keycode, false);
    }
}

void command_processor_thread() {
    while (true) {
        uint32_t read_idx = shared_data->read_index.load(std::memory_order_acquire);
        uint32_t write_idx = shared_data->write_index.load(std::memory_order_acquire);
        
        if (read_idx != write_idx) {
            const auto& cmd = shared_data->command_buffer[read_idx];
            CommandType type = cmd.type.load(std::memory_order_relaxed);

            if (type == CMD_KEY_ACTION) {
                uint8_t win_vk = cmd.win_vk_code.load(std::memory_order_relaxed);
                uint16_t macos_key = win_vkey_to_macos_key(win_vk);
                int32_t value = cmd.value.load(std::memory_order_relaxed);
                
                if (macos_key == MACOS_MOUSE_LEFT) {
                    emit_mouse_button_event(kCGMouseButtonLeft, kCGEventLeftMouseDown, kCGEventLeftMouseUp, value != 0);
                } else if (macos_key == MACOS_MOUSE_RIGHT) {
                    emit_mouse_button_event(kCGMouseButtonRight, kCGEventRightMouseDown, kCGEventRightMouseUp, value != 0);
                } else if (macos_key == MACOS_MOUSE_MIDDLE) {
                    emit_mouse_button_event(kCGMouseButtonCenter, kCGEventOtherMouseDown, kCGEventOtherMouseUp, value != 0);
                } else if (macos_key != MACOS_UNASSIGNED) {
                    printf("[Helper] Emitting key event: VK=0x%02X macOS=%d VAL=%d\n", win_vk, macos_key, value);
                    emit_key_event(macos_key, value != 0);
                }
            } else if (type == CMD_MOUSE_MOVE_RELATIVE) {
                int32_t dx = cmd.rel_x.load(std::memory_order_relaxed);
                int32_t dy = cmd.rel_y.load(std::memory_order_relaxed);
                emit_mouse_move_event(dx, dy);
            } else if (type == CMD_MOUSE_WHEEL) {
                int32_t delta = cmd.value.load(std::memory_order_relaxed);
                printf("[Helper] Emitting mouse wheel event: DELTA=%d\n", delta);
                emit_mouse_wheel_event(delta);
            } else if (type == CMD_SUSPEND_PROCESS) {
                pid_t pid = cmd.target_pid.load(std::memory_order_relaxed);
                printf("[Helper] Received CMD_SUSPEND_PROCESS for PID: %d\n", pid);
                if (pid > 0) {
                    hybrid_suspend_or_resume(pid, true);
                }
            } else if (type == CMD_RESUME_PROCESS) {
                pid_t pid = cmd.target_pid.load(std::memory_order_relaxed);
                printf("[Helper] Received CMD_RESUME_PROCESS for PID: %d\n", pid);
                if (pid > 0) {
                    hybrid_suspend_or_resume(pid, false);
                }
            } else if (type == CMD_BHOP_ENABLE) {
                printf("[Helper] Received CMD_BHOP_ENABLE\n");
                isbhop = true;
            } else if (type == CMD_BHOP_DISABLE) {
                printf("[Helper] Received CMD_BHOP_DISABLE\n");
                isbhop = false;
            } else if (type == CMD_DESYNC_ENABLE) {
                printf("[Helper] Received CMD_DESYNC_ENABLE\n");
                isdesync = true;
            } else if (type == CMD_DESYNC_DISABLE) {
                printf("[Helper] Received CMD_DESYNC_DISABLE\n");
                isdesync = false;
            }

            shared_data->read_index.store((read_idx + 1) & (COMMAND_BUFFER_SIZE - 1), std::memory_order_release);
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }
}

void special_action_thread() {
    while (true) {
        if (shared_data->special_action.request_ready.load(std::memory_order_acquire)) {
            shared_data->special_action.request_ready.store(false, std::memory_order_relaxed);
            SpecialActionCommand cmd = shared_data->special_action.command.load(std::memory_order_relaxed);
            bool success = false;
            int pid_count = 0;
            
            std::string name(shared_data->special_action.process_name);

            if (cmd == SA_FIND_NEWEST_PROCESS) {
                pid_t pid = find_main_process_by_name(name);
                if (pid != -1) {
                    shared_data->special_action.response_pids[0] = pid;
                    pid_count = 1;
                    success = true;
                }
            } else if (cmd == SA_FIND_ALL_PROCESSES) {
                std::vector<pid_t> pids = find_all_processes_by_exes_or_pids(name);
                if (!pids.empty()) {
                    for (size_t i = 0; i < pids.size() && i < 128; ++i) {
                        shared_data->special_action.response_pids[i] = pids[i];
                        pid_count++;
                    }
                    success = true;
                }
            } else if (cmd == SA_SET_BHOP_DELAY) {
                int32_t delay = shared_data->special_action.response_pid_count.load(std::memory_order_relaxed);
                if (delay >= 0) {
                    bhop_delay = delay;
                    success = true;
                    printf("[Helper] Bhop delay set to %d ms\n", bhop_delay);
                }
            } else if (cmd == SA_SET_DESYNC_ITEM) {
                int32_t itemslot = shared_data->special_action.response_pid_count.load(std::memory_order_relaxed);
                if (itemslot >= 1 && itemslot <= 9) {
                    desync_itemslot = itemslot;
                    success = true;
                    printf("[Helper] Desync item slot set to %d\n", desync_itemslot);
                }
            }
            
            shared_data->special_action.response_pid_count.store(pid_count, std::memory_order_relaxed);
            shared_data->special_action.response_success.store(success, std::memory_order_relaxed);
            shared_data->special_action.response_ready.store(true, std::memory_order_release);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// ********************************************************************
// *                  PROCESS FINDING FUNCTIONS                       *
// ********************************************************************

std::vector<pid_t> find_all_processes_by_exes_or_pids(const std::string& input) {
    std::vector<pid_t> pids;
    std::vector<std::string> exe_names;
    std::vector<pid_t> pid_tokens;

    // Split input by spaces
    std::istringstream iss(input);
    std::string token;
    while (iss >> token) {
        bool is_pid = true;
        for (char c : token) {
            if (!isdigit(c)) {
                is_pid = false;
                break;
            }
        }

        if (is_pid) {
            try {
                pid_t pid = std::stoi(token);
                if (pid > 0) pid_tokens.push_back(pid);
            } catch (...) {}
        } else {
            exe_names.push_back(token);
        }
    }

    // Add valid PIDs directly
    pids.insert(pids.end(), pid_tokens.begin(), pid_tokens.end());

    // Check if PIDs are valid processes
    pids.erase(std::remove_if(pids.begin(), pids.end(), [](pid_t pid) {
        int result = kill(pid, 0);
        bool invalid = (result != 0 && errno != EPERM);
        
        printf("[Helper] Validating PID: %d - %s\n", pid, invalid ? "Invalid" : "Valid");
        
        if (!invalid) {
            char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
            if (proc_pidpath(pid, pathbuf, sizeof(pathbuf)) > 0) {
                printf("[Helper] PID: %d - Path: %s\n", pid, pathbuf);
            }
        }
        
        return invalid;
    }), pids.end());

    // If there are no executable names, we're done
    if (exe_names.empty()) return pids;

    // Get all running processes on macOS
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t size;
    
    if (sysctl(mib, 4, NULL, &size, NULL, 0) < 0) {
        return pids;
    }
    
    struct kinfo_proc* processes = (struct kinfo_proc*)malloc(size);
    if (!processes) return pids;
    
    if (sysctl(mib, 4, processes, &size, NULL, 0) < 0) {
        free(processes);
        return pids;
    }
    
    int process_count = size / sizeof(struct kinfo_proc);
    
    for (int i = 0; i < process_count; i++) {
        pid_t pid = processes[i].kp_proc.p_pid;
        if (pid <= 0) continue;
        
        char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
        if (proc_pidpath(pid, pathbuf, sizeof(pathbuf)) <= 0) continue;
        
        std::string path(pathbuf);
        
        for (const auto& exe : exe_names) {
            if (path.size() >= exe.size() &&
                path.compare(path.size() - exe.size(), exe.size(), exe) == 0) {
                pids.push_back(pid);
                break;
            }
        }
    }
    
    free(processes);
    return pids;
}

pid_t find_main_process_by_name(const std::string& name) {
    std::vector<pid_t> pids = find_all_processes_by_exes_or_pids(name);
    if (pids.empty()) {
        return -1;
    }
    if (pids.size() == 1) {
        return pids[0];
    }

    std::set<pid_t> pid_set(pids.begin(), pids.end());

    for (pid_t pid : pids) {
        pid_t ppid = get_ppid(pid);
        if (pid_set.find(ppid) == pid_set.end()) {
            return pid;
        }
    }

    return *std::min_element(pids.begin(), pids.end());
}
