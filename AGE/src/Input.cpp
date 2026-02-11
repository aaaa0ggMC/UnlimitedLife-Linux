#include <AGE/Input.h>
#include <AGE/Window.h>

using namespace age;

void Input::update(){
    if(!trig.test() || !win)return;
    trig.reset();
    ticked = true;
    for(auto & [keycode,keyinfo] : codes){
        bool down = glfwGetKey(win->getSystemHandle(),static_cast<int>(keycode)) == GLFW_PRESS;

        switch(keyinfo.status){
        case KeyState::Released:
            if(down){
                keyinfo.status = KeyState::PressedThisTick;
                keyinfo.this_pressed = rater.get_all();
                if(keyinfo.double_time_ms > 0){
                    if(keyinfo.this_pressed - keyinfo.last_pressed >= keyinfo.double_time_ms){
                        keyinfo.last_pressed = keyinfo.this_pressed;
                    }
                }
            }
            break;
        case KeyState::PressedThisTick:
            if(down)keyinfo.status = KeyState::Held;
            else keyinfo.status = KeyState::ReleasedThisTick;
            break;
        case KeyState::Held:
            if(!down)keyinfo.status = KeyState::ReleasedThisTick;
            break;
        case KeyState::ReleasedThisTick:
            if(!down){
                keyinfo.status = KeyState::Released;
            }else {
                keyinfo.status = KeyState::PressedThisTick;
                keyinfo.this_pressed = rater.get_all();
                if(keyinfo.double_time_ms > 0){
                    if(keyinfo.this_pressed - keyinfo.last_pressed >= keyinfo.double_time_ms){
                        keyinfo.last_pressed = keyinfo.this_pressed;
                    }
                }
            }
            break;
        }
    }
}

std::string_view  KeyWrapper::getKeyCodeString() const {
    static const std::unordered_map<int, std::string> keyNameMap = [] {
        std::unordered_map<int, std::string> map;
        // 字母键
        for (int k = GLFW_KEY_A; k <= GLFW_KEY_Z; ++k) {
            map[k] = static_cast<char>('A' + (k - GLFW_KEY_A));
        }
        // 数字键
        for (int k = GLFW_KEY_0; k <= GLFW_KEY_9; ++k) {
            map[k] = static_cast<char>('0' + (k - GLFW_KEY_0));
        }
        // 功能键
        map[GLFW_KEY_SPACE] = "Space";
        map[GLFW_KEY_ENTER] = "Enter";
        map[GLFW_KEY_ESCAPE] = "Escape";
        map[GLFW_KEY_TAB] = "Tab";
        map[GLFW_KEY_BACKSPACE] = "Backspace";
        map[GLFW_KEY_INSERT] = "Insert";
        map[GLFW_KEY_DELETE] = "Delete";
        map[GLFW_KEY_RIGHT] = "Right";
        map[GLFW_KEY_LEFT] = "Left";
        map[GLFW_KEY_DOWN] = "Down";
        map[GLFW_KEY_UP] = "Up";
        map[GLFW_KEY_PAGE_UP] = "PageUp";
        map[GLFW_KEY_PAGE_DOWN] = "PageDown";
        map[GLFW_KEY_HOME] = "Home";
        map[GLFW_KEY_END] = "End";
        map[GLFW_KEY_CAPS_LOCK] = "CapsLock";
        map[GLFW_KEY_SCROLL_LOCK] = "ScrollLock";
        map[GLFW_KEY_NUM_LOCK] = "NumLock";
        map[GLFW_KEY_PRINT_SCREEN] = "PrintScreen";
        map[GLFW_KEY_PAUSE] = "Pause";
        // F1-F25
        for (int k = GLFW_KEY_F1; k <= GLFW_KEY_F25; ++k) {
            map[k] = "F" + std::to_string(k - GLFW_KEY_F1 + 1);
        }
        // 小键盘键
        map[GLFW_KEY_KP_0] = "Num0";
        map[GLFW_KEY_KP_1] = "Num1";
        map[GLFW_KEY_KP_2] = "Num2";
        map[GLFW_KEY_KP_3] = "Num3";
        map[GLFW_KEY_KP_4] = "Num4";
        map[GLFW_KEY_KP_5] = "Num5";
        map[GLFW_KEY_KP_6] = "Num6";
        map[GLFW_KEY_KP_7] = "Num7";
        map[GLFW_KEY_KP_8] = "Num8";
        map[GLFW_KEY_KP_9] = "Num9";
        map[GLFW_KEY_KP_DECIMAL] = "NumDecimal";
        map[GLFW_KEY_KP_DIVIDE] = "NumDivide";
        map[GLFW_KEY_KP_MULTIPLY] = "NumMultiply";
        map[GLFW_KEY_KP_SUBTRACT] = "NumSubtract";
        map[GLFW_KEY_KP_ADD] = "NumAdd";
        map[GLFW_KEY_KP_ENTER] = "NumEnter";
        map[GLFW_KEY_KP_EQUAL] = "NumEqual";
        // 符号键
        map[GLFW_KEY_GRAVE_ACCENT] = "`";
        map[GLFW_KEY_MINUS] = "-";
        map[GLFW_KEY_EQUAL] = "=";
        map[GLFW_KEY_LEFT_BRACKET] = "[";
        map[GLFW_KEY_RIGHT_BRACKET] = "]";
        map[GLFW_KEY_BACKSLASH] = "\\";
        map[GLFW_KEY_SEMICOLON] = ";";
        map[GLFW_KEY_APOSTROPHE] = "'";
        map[GLFW_KEY_COMMA] = ",";
        map[GLFW_KEY_PERIOD] = ".";
        map[GLFW_KEY_SLASH] = "/";
        map[GLFW_KEY_LEFT_SHIFT] = "LSHIFT";
        map[GLFW_KEY_LEFT_CONTROL] = "LCTRL";
        map[GLFW_KEY_LEFT_ALT] = "LALT";
        map[GLFW_KEY_LEFT_SUPER] = "LSUPER";
        map[GLFW_KEY_RIGHT_SHIFT] = "RSHIFT";
        map[GLFW_KEY_RIGHT_CONTROL] = "RCTRL";
        map[GLFW_KEY_RIGHT_ALT] = "RALT";
        map[GLFW_KEY_RIGHT_SUPER] = "RSUPER";
        map[GLFW_KEY_MENU] = "MENU";
        return map;
    }();
    thread_local static std::string cachedName;

    // use my keycode
    /*
    if(const char* glfwName = glfwGetKeyName(key, scancode)) {
        cachedName = glfwName;
        return cachedName;
    }
    */
    if (auto it = keyNameMap.find(key); it != keyNameMap.end()) {
        return it->second;
    }

    cachedName = "Key" + std::to_string(key);
    return cachedName;
}
