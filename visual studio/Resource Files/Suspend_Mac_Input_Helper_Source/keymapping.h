#ifndef KEY_MAPPING_H
#define KEY_MAPPING_H

#include <cstdint>
#include <map>

// Windows Virtual-Key Codes can be found here:
// https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes

// macOS Virtual-Key Codes (Quartz Event Services key codes)
// Reference: https://developer.apple.com/documentation/coregraphics/1572112-cgeventkeyboardgetunickeysbuffer

// Unassigned key code
const uint8_t VK_UNASSIGNED = 0x0F;

// Dummy macOS key codes for mouse buttons
const uint16_t MACOS_MOUSE_LEFT = 0xFF;
const uint16_t MACOS_MOUSE_RIGHT = 0xFE;
const uint16_t MACOS_MOUSE_MIDDLE = 0xFD;
const uint16_t MACOS_MOUSE_X1 = 0xFC;
const uint16_t MACOS_MOUSE_X2 = 0xFB;
const uint16_t MACOS_UNASSIGNED = 0xFFFF;

/**
 * @brief Maps a macOS Quartz key code to a Windows virtual key code.
 *
 * This function takes a macOS Quartz key code and returns the
 * corresponding Windows virtual-key code. If a direct mapping is not
 * available, VK_UNASSIGNED (0x0F) is returned.
 *
 * @param macos_key_code The macOS Quartz key code.
 * @return The corresponding Windows virtual key code, or VK_UNASSIGNED.
 */
uint8_t macos_to_win_vkey(uint16_t macos_key_code) {
    switch (macos_key_code) {
        case 0x00: return 0x41; // kVK_ANSI_A -> 'A'
        case 0x01: return 0x53; // kVK_ANSI_S -> 'S'
        case 0x02: return 0x44; // kVK_ANSI_D -> 'D'
        case 0x03: return 0x46; // kVK_ANSI_F -> 'F'
        case 0x04: return 0x48; // kVK_ANSI_H -> 'H'
        case 0x05: return 0x47; // kVK_ANSI_G -> 'G'
        case 0x06: return 0x5A; // kVK_ANSI_Z -> 'Z'
        case 0x07: return 0x58; // kVK_ANSI_X -> 'X'
        case 0x08: return 0x43; // kVK_ANSI_C -> 'C'
        case 0x09: return 0x56; // kVK_ANSI_V -> 'V'
        case 0x0B: return 0x42; // kVK_ANSI_B -> 'B'
        case 0x0C: return 0x51; // kVK_ANSI_Q -> 'Q'
        case 0x0D: return 0x57; // kVK_ANSI_W -> 'W'
        case 0x0E: return 0x45; // kVK_ANSI_E -> 'E'
        case 0x0F: return 0x52; // kVK_ANSI_R -> 'R'
        case 0x10: return 0x59; // kVK_ANSI_Y -> 'Y'
        case 0x11: return 0x54; // kVK_ANSI_T -> 'T'
        case 0x12: return 0x31; // kVK_ANSI_1 -> '1'
        case 0x13: return 0x32; // kVK_ANSI_2 -> '2'
        case 0x14: return 0x33; // kVK_ANSI_3 -> '3'
        case 0x15: return 0x34; // kVK_ANSI_4 -> '4'
        case 0x16: return 0x36; // kVK_ANSI_6 -> '6'
        case 0x17: return 0x35; // kVK_ANSI_5 -> '5'
        case 0x18: return 0xBB; // kVK_ANSI_Equal -> VK_OEM_PLUS
        case 0x19: return 0x39; // kVK_ANSI_9 -> '9'
        case 0x1A: return 0x37; // kVK_ANSI_7 -> '7'
        case 0x1B: return 0xBD; // kVK_ANSI_Minus -> VK_OEM_MINUS
        case 0x1C: return 0x38; // kVK_ANSI_8 -> '8'
        case 0x1D: return 0x30; // kVK_ANSI_0 -> '0'
        case 0x1E: return 0xDD; // kVK_ANSI_RightBracket -> VK_OEM_6
        case 0x1F: return 0x4F; // kVK_ANSI_O -> 'O'
        case 0x20: return 0x55; // kVK_ANSI_U -> 'U'
        case 0x21: return 0xDB; // kVK_ANSI_LeftBracket -> VK_OEM_4
        case 0x22: return 0x49; // kVK_ANSI_I -> 'I'
        case 0x23: return 0x50; // kVK_ANSI_P -> 'P'
        case 0x24: return 0x0D; // kVK_Return -> VK_RETURN
        case 0x25: return 0x4C; // kVK_ANSI_L -> 'L'
        case 0x26: return 0x4A; // kVK_ANSI_J -> 'J'
        case 0x27: return 0xDE; // kVK_ANSI_Quote -> VK_OEM_7
        case 0x28: return 0x4B; // kVK_ANSI_K -> 'K'
        case 0x29: return 0xBA; // kVK_ANSI_Semicolon -> VK_OEM_1
        case 0x2A: return 0xDC; // kVK_ANSI_Backslash -> VK_OEM_5
        case 0x2B: return 0xBC; // kVK_ANSI_Comma -> VK_OEM_COMMA
        case 0x2C: return 0xBF; // kVK_ANSI_Slash -> VK_OEM_2
        case 0x2D: return 0x4E; // kVK_ANSI_N -> 'N'
        case 0x2E: return 0x4D; // kVK_ANSI_M -> 'M'
        case 0x2F: return 0xBE; // kVK_ANSI_Period -> VK_OEM_PERIOD
        case 0x30: return 0x09; // kVK_Tab -> VK_TAB
        case 0x31: return 0x20; // kVK_Space -> VK_SPACE
        case 0x32: return 0xC0; // kVK_ANSI_Grave -> VK_OEM_3
        case 0x33: return 0x08; // kVK_Delete -> VK_BACK
        case 0x35: return 0x1B; // kVK_Escape -> VK_ESCAPE
        case 0x37: return 0x5B; // kVK_Command -> VK_LWIN
        case 0x38: return 0x10; // kVK_Shift -> VK_SHIFT
        case 0x39: return 0x14; // kVK_CapsLock -> VK_CAPITAL
        case 0x3A: return 0x12; // kVK_Option -> VK_MENU
        case 0x3B: return 0x11; // kVK_Control -> VK_CONTROL
        case 0x3C: return 0x10; // kVK_RightShift -> VK_RSHIFT
        case 0x3D: return 0x12; // kVK_RightOption -> VK_RMENU
        case 0x3E: return 0x11; // kVK_RightControl -> VK_RCONTROL
        case 0x41: return 0x6E; // kVK_ANSI_KeypadDecimal -> VK_DECIMAL
        case 0x43: return 0x6A; // kVK_ANSI_KeypadMultiply -> VK_MULTIPLY
        case 0x45: return 0x6B; // kVK_ANSI_KeypadPlus -> VK_ADD
        case 0x4B: return 0x6F; // kVK_ANSI_KeypadDivide -> VK_DIVIDE
        case 0x4C: return 0x0D; // kVK_ANSI_KeypadEnter -> VK_RETURN
        case 0x4E: return 0x6D; // kVK_ANSI_KeypadMinus -> VK_SUBTRACT
        case 0x52: return 0x60; // kVK_ANSI_Keypad0 -> VK_NUMPAD0
        case 0x53: return 0x61; // kVK_ANSI_Keypad1 -> VK_NUMPAD1
        case 0x54: return 0x62; // kVK_ANSI_Keypad2 -> VK_NUMPAD2
        case 0x55: return 0x63; // kVK_ANSI_Keypad3 -> VK_NUMPAD3
        case 0x56: return 0x64; // kVK_ANSI_Keypad4 -> VK_NUMPAD4
        case 0x57: return 0x65; // kVK_ANSI_Keypad5 -> VK_NUMPAD5
        case 0x58: return 0x66; // kVK_ANSI_Keypad6 -> VK_NUMPAD6
        case 0x59: return 0x67; // kVK_ANSI_Keypad7 -> VK_NUMPAD7
        case 0x5B: return 0x68; // kVK_ANSI_Keypad8 -> VK_NUMPAD8
        case 0x5C: return 0x69; // kVK_ANSI_Keypad9 -> VK_NUMPAD9
        case 0x60: return 0x74; // kVK_F5 -> VK_F5
        case 0x61: return 0x75; // kVK_F6 -> VK_F6
        case 0x62: return 0x76; // kVK_F7 -> VK_F7
        case 0x63: return 0x72; // kVK_F3 -> VK_F3
        case 0x64: return 0x77; // kVK_F8 -> VK_F8
        case 0x65: return 0x78; // kVK_F9 -> VK_F9
        case 0x67: return 0x7A; // kVK_F11 -> VK_F11
        case 0x69: return 0x7C; // kVK_F13 -> VK_F13
        case 0x6B: return 0x7D; // kVK_F14 -> VK_F14
        case 0x6D: return 0x79; // kVK_F10 -> VK_F10
        case 0x6F: return 0x7B; // kVK_F12 -> VK_F12
        case 0x71: return 0x7E; // kVK_F15 -> VK_F15
        case 0x72: return 0x2D; // kVK_Help -> VK_INSERT
        case 0x73: return 0x24; // kVK_Home -> VK_HOME
        case 0x74: return 0x21; // kVK_PageUp -> VK_PRIOR
        case 0x75: return 0x2E; // kVK_ForwardDelete -> VK_DELETE
        case 0x76: return 0x73; // kVK_F4 -> VK_F4
        case 0x77: return 0x23; // kVK_End -> VK_END
        case 0x78: return 0x71; // kVK_F2 -> VK_F2
        case 0x79: return 0x22; // kVK_PageDown -> VK_NEXT
        case 0x7A: return 0x70; // kVK_F1 -> VK_F1
        case 0x7B: return 0x25; // kVK_LeftArrow -> VK_LEFT
        case 0x7C: return 0x27; // kVK_RightArrow -> VK_RIGHT
        case 0x7D: return 0x28; // kVK_DownArrow -> VK_DOWN
        case 0x7E: return 0x26; // kVK_UpArrow -> VK_UP

        default:   return VK_UNASSIGNED;
    }
}

/**
 * @brief Maps a Windows virtual key code to a macOS Quartz key code.
 *
 * This function takes a Windows virtual key code and returns the
 * corresponding macOS Quartz key code. For mouse buttons, it returns
 * custom dummy key codes (MACOS_MOUSE_LEFT, MACOS_MOUSE_RIGHT, MACOS_MOUSE_MIDDLE)
 * that your macOS application can handle.
 * If a direct mapping is not available, MACOS_UNASSIGNED is returned.
 *
 * @param win_vkey_code The Windows virtual key code.
 * @return The corresponding macOS Quartz key code, or MACOS_UNASSIGNED.
 */
uint16_t win_vkey_to_macos_key(uint8_t win_vkey_code) {
    switch (win_vkey_code) {
        // Mouse Buttons (using custom dummy macOS key codes)
        case 0x01: return MACOS_MOUSE_LEFT;   // VK_LBUTTON
        case 0x02: return MACOS_MOUSE_RIGHT;  // VK_RBUTTON
        case 0x04: return MACOS_MOUSE_MIDDLE; // VK_MBUTTON
        case 0x05: return MACOS_MOUSE_X1; // VK_XBUTTON1
        case 0x06: return MACOS_MOUSE_X2; // VK_XBUTTON2

        // Keyboard Keys (inverse of macos_to_win_vkey)
        case 0x41: return 0x00; // 'A' -> kVK_ANSI_A
        case 0x53: return 0x01; // 'S' -> kVK_ANSI_S
        case 0x44: return 0x02; // 'D' -> kVK_ANSI_D
        case 0x46: return 0x03; // 'F' -> kVK_ANSI_F
        case 0x48: return 0x04; // 'H' -> kVK_ANSI_H
        case 0x47: return 0x05; // 'G' -> kVK_ANSI_G
        case 0x5A: return 0x06; // 'Z' -> kVK_ANSI_Z
        case 0x58: return 0x07; // 'X' -> kVK_ANSI_X
        case 0x43: return 0x08; // 'C' -> kVK_ANSI_C
        case 0x56: return 0x09; // 'V' -> kVK_ANSI_V
        case 0x42: return 0x0B; // 'B' -> kVK_ANSI_B
        case 0x51: return 0x0C; // 'Q' -> kVK_ANSI_Q
        case 0x57: return 0x0D; // 'W' -> kVK_ANSI_W
        case 0x45: return 0x0E; // 'E' -> kVK_ANSI_E
        case 0x52: return 0x0F; // 'R' -> kVK_ANSI_R
        case 0x59: return 0x10; // 'Y' -> kVK_ANSI_Y
        case 0x54: return 0x11; // 'T' -> kVK_ANSI_T
        case 0x31: return 0x12; // '1' -> kVK_ANSI_1
        case 0x32: return 0x13; // '2' -> kVK_ANSI_2
        case 0x33: return 0x14; // '3' -> kVK_ANSI_3
        case 0x34: return 0x15; // '4' -> kVK_ANSI_4
        case 0x36: return 0x16; // '6' -> kVK_ANSI_6
        case 0x35: return 0x17; // '5' -> kVK_ANSI_5
        case 0xBB: return 0x18; // VK_OEM_PLUS -> kVK_ANSI_Equal
        case 0x39: return 0x19; // '9' -> kVK_ANSI_9
        case 0x37: return 0x1A; // '7' -> kVK_ANSI_7
        case 0xBD: return 0x1B; // VK_OEM_MINUS -> kVK_ANSI_Minus
        case 0x38: return 0x1C; // '8' -> kVK_ANSI_8
        case 0x30: return 0x1D; // '0' -> kVK_ANSI_0
        case 0xDD: return 0x1E; // VK_OEM_6 -> kVK_ANSI_RightBracket
        case 0x4F: return 0x1F; // 'O' -> kVK_ANSI_O
        case 0x55: return 0x20; // 'U' -> kVK_ANSI_U
        case 0xDB: return 0x21; // VK_OEM_4 -> kVK_ANSI_LeftBracket
        case 0x49: return 0x22; // 'I' -> kVK_ANSI_I
        case 0x50: return 0x23; // 'P' -> kVK_ANSI_P
        case 0x0D: return 0x24; // VK_RETURN -> kVK_Return
        case 0x4C: return 0x25; // 'L' -> kVK_ANSI_L
        case 0x4A: return 0x26; // 'J' -> kVK_ANSI_J
        case 0xDE: return 0x27; // VK_OEM_7 -> kVK_ANSI_Quote
        case 0x4B: return 0x28; // 'K' -> kVK_ANSI_K
        case 0xBA: return 0x29; // VK_OEM_1 -> kVK_ANSI_Semicolon
        case 0xDC: return 0x2A; // VK_OEM_5 -> kVK_ANSI_Backslash
        case 0xBC: return 0x2B; // VK_OEM_COMMA -> kVK_ANSI_Comma
        case 0xBF: return 0x2C; // VK_OEM_2 -> kVK_ANSI_Slash
        case 0x4E: return 0x2D; // 'N' -> kVK_ANSI_N
        case 0x4D: return 0x2E; // 'M' -> kVK_ANSI_M
        case 0xBE: return 0x2F; // VK_OEM_PERIOD -> kVK_ANSI_Period
        case 0x09: return 0x30; // VK_TAB -> kVK_Tab
        case 0x20: return 0x31; // VK_SPACE -> kVK_Space
        case 0xC0: return 0x32; // VK_OEM_3 -> kVK_ANSI_Grave
        case 0x08: return 0x33; // VK_BACK -> kVK_Delete
        case 0x1B: return 0x35; // VK_ESCAPE -> kVK_Escape
        case 0x5B: return 0x37; // VK_LWIN -> kVK_Command
        case 0x10: return 0x38; // VK_SHIFT (L or R) -> kVK_Shift (or kVK_RightShift if differentiating)
        case 0x14: return 0x39; // VK_CAPITAL -> kVK_CapsLock
        case 0x12: return 0x3A; // VK_MENU (L or R) -> kVK_Option (or kVK_RightOption if differentiating)
        case 0x11: return 0x3B; // VK_CONTROL (L or R) -> kVK_Control (or kVK_RightControl if differentiating)
        // case 0x10 (VK_RSHIFT) maps to 0x3C, but 0x10 (VK_LSHIFT) already maps to 0x38.
        // Prioritizing the generic SHIFT, you might need more complex logic if L/R SHIFT are distinct in your usage.
        // For simplicity, using the first mapping for shared VKs.
        // case 0x12 (VK_RMENU) maps to 0x3D
        // case 0x11 (VK_RCONTROL) maps to 0x3E
        case 0x6E: return 0x41; // VK_DECIMAL -> kVK_ANSI_KeypadDecimal
        case 0x6A: return 0x43; // VK_MULTIPLY -> kVK_ANSI_KeypadMultiply
        case 0x6B: return 0x45; // VK_ADD -> kVK_ANSI_KeypadPlus
        case 0x6F: return 0x4B; // VK_DIVIDE -> kVK_ANSI_KeypadDivide
        case 0x6D: return 0x4E; // VK_SUBTRACT -> kVK_ANSI_KeypadMinus
        case 0x60: return 0x52; // VK_NUMPAD0 -> kVK_ANSI_Keypad0
        case 0x61: return 0x53; // VK_NUMPAD1 -> kVK_ANSI_Keypad1
        case 0x62: return 0x54; // VK_NUMPAD2 -> kVK_ANSI_Keypad2
        case 0x63: return 0x55; // VK_NUMPAD3 -> kVK_ANSI_Keypad3
        case 0x64: return 0x56; // VK_NUMPAD4 -> kVK_ANSI_Keypad4
        case 0x65: return 0x57; // VK_NUMPAD5 -> kVK_ANSI_Keypad5
        case 0x66: return 0x58; // VK_NUMPAD6 -> kVK_ANSI_Keypad6
        case 0x67: return 0x59; // VK_NUMPAD7 -> kVK_ANSI_Keypad7
        case 0x68: return 0x5B; // VK_NUMPAD8 -> kVK_ANSI_Keypad8
        case 0x69: return 0x5C; // VK_NUMPAD9 -> kVK_ANSI_Keypad9
        case 0x74: return 0x60; // VK_F5 -> kVK_F5
        case 0x75: return 0x61; // VK_F6 -> kVK_F6
        case 0x76: return 0x62; // VK_F7 -> kVK_F7
        case 0x72: return 0x63; // VK_F3 -> kVK_F3
        case 0x77: return 0x64; // VK_F8 -> kVK_F8
        case 0x78: return 0x65; // VK_F9 -> kVK_F9
        case 0x7A: return 0x67; // VK_F11 -> kVK_F11
        case 0x7C: return 0x69; // VK_F13 -> kVK_F13
        case 0x7D: return 0x6B; // VK_F14 -> kVK_F14
        case 0x79: return 0x6D; // VK_F10 -> kVK_F10
        case 0x7B: return 0x6F; // VK_F12 -> kVK_F12
        case 0x7E: return 0x71; // VK_F15 -> kVK_F15
        case 0x2D: return 0x72; // VK_INSERT -> kVK_Help (macOS 'Help' is often 'Insert' on Windows)
        case 0x24: return 0x73; // VK_HOME -> kVK_Home
        case 0x21: return 0x74; // VK_PRIOR -> kVK_PageUp
        case 0x2E: return 0x75; // VK_DELETE -> kVK_ForwardDelete
        case 0x73: return 0x76; // VK_F4 -> kVK_F4
        case 0x23: return 0x77; // VK_END -> kVK_End
        case 0x71: return 0x78; // VK_F2 -> kVK_F2
        case 0x22: return 0x79; // VK_NEXT -> kVK_PageDown
        case 0x70: return 0x7A; // VK_F1 -> kVK_F1
        case 0x25: return 0x7B; // VK_LEFT -> kVK_LeftArrow
        case 0x27: return 0x7C; // VK_RIGHT -> kVK_RightArrow
        case 0x28: return 0x7D; // VK_DOWN -> kVK_DownArrow
        case 0x26: return 0x7E; // VK_UP -> kVK_UpArrow

        default:   return MACOS_UNASSIGNED;
    }
}

#endif // KEY_MAPPING_H