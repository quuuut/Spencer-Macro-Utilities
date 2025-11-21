#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <csignal>
#include <IOKit/hid/IOHIDUserDevice.h>
#include <CoreFoundation/CoreFoundation.h>
#include "shared_mem.h"
#include "keymapping.h"

// --- Globals ---
SharedMemory* shared_data = nullptr;
IOHIDUserDeviceRef virtual_keyboard = nullptr;
IOHIDUserDeviceRef virtual_mouse = nullptr;
bool isbhop = false;
bool isdesync = false;
int bhop_delay = 10;
int desync_itemslot = 5;

void cleanup(int signum) {
    std::cout << "Cleaning up..." << std::endl;
    if (virtual_keyboard) CFRelease(virtual_keyboard);
    if (virtual_mouse) CFRelease(virtual_mouse);
    exit(0);
}

IOHIDUserDeviceRef create_virtual_device(uint16_t usage_page, uint16_t usage) {
    CFMutableDictionaryRef device_properties = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );

    CFNumberRef vendor_id = CFNumberCreate(NULL, kCFNumberIntType, &(int){1234});
    CFNumberRef product_id = CFNumberCreate(NULL, kCFNumberIntType, &(int){5678});
    CFNumberRef usage_page_ref = CFNumberCreate(NULL, kCFNumberIntType, &usage_page);
    CFNumberRef usage_ref = CFNumberCreate(NULL, kCFNumberIntType, &usage);

    CFDictionarySetValue(device_properties, CFSTR(kIOHIDVendorIDKey), vendor_id);
    CFDictionarySetValue(device_properties, CFSTR(kIOHIDProductIDKey), product_id);
    CFDictionarySetValue(device_properties, CFSTR(kIOHIDPrimaryUsagePageKey), usage_page_ref);
    CFDictionarySetValue(device_properties, CFSTR(kIOHIDPrimaryUsageKey), usage_ref);

    IOHIDUserDeviceRef device = IOHIDUserDeviceCreate(kCFAllocatorDefault, device_properties);

    CFRelease(device_properties);
    CFRelease(vendor_id);
    CFRelease(product_id);
    CFRelease(usage_page_ref);
    CFRelease(usage_ref);

    if (!device) {
        std::cerr << "Failed to create virtual HID device." << std::endl;
        exit(1);
    }

    return device;
}

void send_keyboard_key(uint8_t keycode, bool pressed) {
    uint8_t report[8] = {0};
    if (pressed) report[2] = keycode;
    IOHIDUserDeviceHandleReport(virtual_keyboard, report, sizeof(report));
}

void send_mouse_event(int32_t dx, int32_t dy, uint8_t buttons, int32_t wheel=0) {
    uint8_t report[4] = {buttons, (uint8_t)dx, (uint8_t)dy, (uint8_t)wheel};
    IOHIDUserDeviceHandleReport(virtual_mouse, report, sizeof(report));
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
                if (macos_key != MACOS_UNASSIGNED) send_keyboard_key(macos_key, value != 0);

            } else if (type == CMD_MOUSE_MOVE_RELATIVE) {
                int32_t dx = cmd.rel_x.load(std::memory_order_relaxed);
                int32_t dy = cmd.rel_y.load(std::memory_order_relaxed);
                send_mouse_event(dx, dy, 0);

            } else if (type == CMD_MOUSE_WHEEL) {
                int32_t delta = cmd.value.load(std::memory_order_relaxed);
                send_mouse_event(0, 0, 0, delta);

            } else if (type == CMD_MOUSE_BUTTON) {
                uint8_t buttons = cmd.value.load(std::memory_order_relaxed);
                send_mouse_event(0,0,buttons);

            } else if (type == CMD_BHOP_ENABLE) { isbhop = true;
            } else if (type == CMD_BHOP_DISABLE) { isbhop = false;
            } else if (type == CMD_DESYNC_ENABLE) { isdesync = true;
            } else if (type == CMD_DESYNC_DISABLE) { isdesync = false; }

            shared_data->read_index.store((read_idx + 1) & (COMMAND_BUFFER_SIZE - 1), std::memory_order_release);
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }
}

void fastkey_thread() {
    while (true) {
        if (isbhop) {
            bool space_pressed = shared_data->key_states[0x20].load(std::memory_order_acquire);
            if (space_pressed) {
                std::this_thread::sleep_for(std::chrono::milliseconds(bhop_delay/2));
                send_keyboard_key(0x31, false);
                std::this_thread::sleep_for(std::chrono::milliseconds(bhop_delay/2));
                send_keyboard_key(0x31, true);
            }
        }
        if (isdesync) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(100));
            uint16_t keycode = (desync_itemslot == 9) ? 0x19 : (0x12 + desync_itemslot - 1);
            send_keyboard_key(keycode, true);
            std::this_thread::sleep_for(std::chrono::nanoseconds(100));
            send_keyboard_key(keycode, false);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) { std::cout << "Usage: " << argv[0] << " <filename>" << std::endl; return 1; }
    signal(SIGINT, cleanup);

    // Initialize virtual devices
    virtual_keyboard = create_virtual_device(0x01, 0x06); // Keyboard
    virtual_mouse = create_virtual_device(0x01, 0x02);    // Mouse

    // Initialize shared memory
    shared_data = SharedMemory::initialize(MM_FILE_PATH);

    std::thread processor(command_processor_thread);
    std::thread fastkey(fastkey_thread);

    processor.join();
    fastkey.join();

    cleanup(0);
    return 0;
}
