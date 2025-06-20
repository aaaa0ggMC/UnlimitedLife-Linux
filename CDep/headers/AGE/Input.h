#ifndef AGE_H_INPUT
#define AGE_H_INPUT
#include <AGE/Base.h>
#include <AGE/Window.h>
#include <alib-g3/aclock.h>

#include <unordered_map>
#include <cstdint>

namespace age {
    enum class KeyState : uint8_t {
        Released = 0,
        Pressed,
        Held,
        ReleasedThisTick
    };

    struct AGE_API KeyInfo{
        KeyState status; ///<键盘状态

        inline KeyInfo(){
            status = KeyState::Released;
        }
    };

#include <AGE/keycode.inl>

    struct AGE_API Input{
        Window* win;
        std::unordered_map<KeyCode,KeyInfo> codes;
        alib::g3::Clock rater;
        alib::g3::Trigger trig;

        void update();

        inline const KeyInfo& getKeyInfo(KeyCode code){
            auto it = codes.try_emplace(code,KeyInfo());
            return it.first->second;
        }

        inline const KeyInfo& getKeyInfo(int code){
            return getKeyInfo(static_cast<KeyCode>(code));
        }

        inline void setWindow(Window & win){
            this->win = &win;
        }

        inline Input(float runRate = 20,Window * window = nullptr):win{window},trig{rater,1000/runRate}{}
    };

    struct AGE_API KeyWrapper{
        GLFWwindow* window;
        int key;
        int scancode;
        int action;
        int mods;

        inline KeyWrapper(GLFWwindow* win, int k, int s, int a,int m)
        : window(win), key(k), scancode(s), mods(m),action{a} {}

        inline KeyCode getKeyCode() const{
            return static_cast<KeyCode>(key);
        }

        inline bool hasShift() const noexcept    { return mods & GLFW_MOD_SHIFT; }
        inline bool hasControl() const noexcept  { return mods & GLFW_MOD_CONTROL; }
        inline bool hasAlt() const noexcept      { return mods & GLFW_MOD_ALT; }
        inline bool hasSuper() const noexcept    { return mods & GLFW_MOD_SUPER; }

        /// uses key & scancode
        const std::string& getKeyCodeString() const;

    };
}

#endif
