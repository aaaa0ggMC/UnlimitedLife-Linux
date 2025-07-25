/**
 * @file Input.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 输入处理
 * @version 0.1
 * @date 2025/07/25
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/06/28 （左右） 
 */
#ifndef AGE_H_INPUT
#define AGE_H_INPUT
#include <AGE/Utils.h>
#include <alib-g3/aclock.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <unordered_map>
#include <cstdint>

namespace age {
    enum class KeyState : uint8_t {
        Released = 0,
        PressedThisTick,
        Held,
        ReleasedThisTick
    };

    struct AGE_API KeyInfo{
        KeyState status; ///<键盘状态
        float double_time_ms; ///<双击时间
        float last_pressed; ///<上一次
        float this_pressed; ///<这一次

        inline KeyInfo(){
            status = KeyState::Released;
            double_time_ms = -1;
        }

        inline bool isPressing(bool invalidDurDoubleTime = false){
            if(!invalidDurDoubleTime)return (status == KeyState::PressedThisTick) || (status == KeyState::Held);
            return ((status == KeyState::PressedThisTick) || (status == KeyState::Held)) && this_pressed != last_pressed;
        }

        inline bool isReleased(){
            return (status == KeyState::Released) || (status == KeyState::ReleasedThisTick);
        }

        inline bool hasDoubleTapped(){
            if(double_time_ms <= 0)return false;
            if(status != KeyState::PressedThisTick)return false;
            std::cout << (this_pressed - last_pressed) << std::endl;
            if(this_pressed != last_pressed && (this_pressed - last_pressed) <= double_time_ms){
                return true;
            }
            last_pressed = this_pressed;
            return false;
        }

    };

#include <AGE/keycode.inl>

    /// 这里要用到Window
    class Window;
    struct AGE_API Input{
        Window* win;
        std::unordered_map<KeyCode,KeyInfo> codes;
        alib::g3::Clock rater;
        alib::g3::Trigger trig;
        bool ticked;

        void update();

        ///call this function will clear the ticked flag
        inline bool checkTick(){
            if(ticked){
                ticked = false;
                return true;
            }
            return false;
        }

        inline KeyInfo& getKeyInfo(KeyCode code){
            auto it = codes.try_emplace(code,KeyInfo());
            return it.first->second;
        }

        inline KeyInfo& getKeyInfo(int code){
            return getKeyInfo(static_cast<KeyCode>(code));
        }

        inline void setKeyDoubleTap(KeyCode code,float time_ms){
            getKeyInfo(code).double_time_ms = time_ms;
        }

        inline void setKeyDoubleTap(int code,float time_ms){
            setKeyDoubleTap(static_cast<KeyCode>(code),time_ms);
        }

        inline void setWindow(Window & win){
            this->win = &win;
        }

        inline Input(float runRate = 30,Window * window = nullptr):win{window},trig{rater,1000/runRate}{
            ticked = false;
        }
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

        inline KeyAction getKeyAction() const {
            return static_cast<KeyAction>(action);
        }

        inline bool hasShift() const noexcept    { return mods & GLFW_MOD_SHIFT; }
        inline bool hasControl() const noexcept  { return mods & GLFW_MOD_CONTROL; }
        inline bool hasAlt() const noexcept      { return mods & GLFW_MOD_ALT; }
        inline bool hasSuper() const noexcept    { return mods & GLFW_MOD_SUPER; }

        /// uses key & scancode
        std::string_view  getKeyCodeString() const;

    };
}

#endif
