/**
 * @file Utils.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 一些工具
 * @version 0.1
 * @date 2025/12/04
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/06/11 （左右） 
 */
#ifndef AGE_H_BASE
#define AGE_H_BASE

#include <cstdint>
#include <vector>
#include <numbers>
#include <functional>
#include <alib-g3/aclock.h>
#include <alib-g3/adebug.h>
#include <functional>
#include <alib-g3/alogger.h>
#include <GL/glew.h>
#include <functional>
#include <AGE/Details/GLObjectMapper.h>
//#include <iostream>

///对象如VAO,VBO为空
#define AGE_NULL_OBJ 0

///For windows support
#ifdef _WIN32
#include <windows.h>
#include <shellscalingapi.h>
#ifndef AGE_API
#ifdef AGE_BUILD_DLL
    #define AGE_API __declspec(dllexport)
#else
    #define AGE_API __declspec(dllimport)
#endif
#endif // BUILD_DLL

#elif __linux__

#include <unistd.h>
#ifndef AGE_API
#ifdef AGE_BUILD_DLL
    #define AGE_API
#else
    #define AGE_API
#endif
#endif
#endif

// Base Error Code [-1,-9999]
#define AGEE_CONFLICT_SID -1
#define AGEE_EMPTY_DATA -2
#define AGEE_WRONG_ENUM -3
#define AGEE_FEATURE_NOT_SUPPORTED -4
#define AGEE_CANT_FIND_SID -5
// Graphics begin from -10000 to -19999
#define AGEE_CONFLICT_SHADER -10000
#define AGEE_SHADER_LOG -10001
#define AGEE_SHADER_FAILED_TO_COMPILE -10002
#define AGEE_SHADER_FAILED_TO_LINK -10003
#define AGEE_OPENGL_DEBUG_MESSAGE -10004
#define AGEE_OPENGL_NO_CONTEXT -10005
#define AGEE_OPENGL_EMPTY_SHADER -10006
#define AGEE_TEXTURE_LOADED -10007
#define AGEE_OPENGL_CREATE_ERROR -10008

#define AGE_CHECK_GL_CONTEXT panic_debug(glfwGetCurrentContext() == nullptr,"There's no valid GL context here!")

namespace age{
    inline constexpr bool AGE_Enable = true;
    inline constexpr bool AGE_Disable = false;

    template<class T> inline constexpr T rad2deg(const T& in){
        return in * (T(180) / std::numbers::pi);
    }

    template<class T> inline constexpr T deg2rad(const T & in){
        return in * (std::numbers::pi / (T)180);
    }

    float AGE_API getMonitorScale();

    struct AGE_API DirtyMarker{
    private:
        bool dirty {true};
        uint64_t dirtymarker_version { 1 };
    public:
        inline void dm_mark(){
            ++dirtymarker_version;
            dirty = true;
        }

        inline bool dm_check(){
            return dirty;
        }

        inline void dm_clear(){
            dirty = false;
        }

        /// @return 返回是否应该更新
        inline bool dm_version(uint64_t& cached){
            if(cached != dirtymarker_version){
                cached = dirtymarker_version;
                return true;
            }
            return false;
        }

        inline uint64_t dm_get_version(){return dirtymarker_version;}
    };
    struct AGE_API ErrorInfo{
        int32_t code;
        const char * message;
        alib::g3::LogLevel level;
    };

    struct AGE_API ErrorInfopp{
        int32_t code;
        std::pmr::string message;
        alib::g3::LogLevel level;
    };
    using TriggerFunc = std::function<void(const ErrorInfopp&)>;
    /** @struct Error
     *  @brief handle errors during many operations
     */
    struct AGE_API Error{
    public:
        TriggerFunc trigger;
        int32_t limit;
        
        static Error def;

        static std::pmr::unsynchronized_pool_resource pool;
        static std::pmr::polymorphic_allocator<char> alloc;

        std::pmr::vector<ErrorInfopp> infos;

        Error();

        void setLimit(int32_t count);

        void setTrigger(TriggerFunc = Error::defTrigger);
        void pushMessage(const ErrorInfo&);

        static void defTrigger(const ErrorInfopp&);

        static void checkOpenGLError();
    };

    template<GLuint in> struct AGE_API ScopedGLState{
        constexpr static const GLuint bd = GLObjectToBindingMapper<in>(); 
        constexpr static const auto bind = GLObjectToBindingFuncMapper<in>();
        GLint prev;
        GLuint prev_uint;

        inline ScopedGLState(GLuint newone){
            glGetIntegerv(bd,&prev);
            prev_uint = (GLuint)prev;
            //std::cout << "Get " << prev << std::endl;
            //]]std::cout << "Bind " << newone<< std::endl;
            if(prev != newone)bind(newone); //少一次bind检测
        }

        inline ~ScopedGLState(){
            bind(prev_uint);
            //std::cout << "Bind " << prev << std::endl;
        }
    };

    struct AGE_API  NonCopyable{
        NonCopyable() = default;
        NonCopyable(const NonCopyable&) = delete;
        NonCopyable& operator=(const NonCopyable&) = delete;
    };

    /// please use it for trivially_copyable
    template<class T> struct DirtyWrapper{
    private:
        T data;
        static_assert(std::is_trivially_copyable_v<T>,"The data wrapped in DirtyWrapper is not trivally copyable!");
        bool dirty { true };
        bool * linked {nullptr};
    public:

        inline void setLinkDirty(bool * b_link){
            linked = b_link;
        }

        inline const T& read() const{
            return data;
        }

        inline void write(const T& val){
            dirty = true;
            if(linked)*linked = true;
            data = val;
        }

        inline void mark(){
            dirty = true;
            if(linked)*linked = true;
        }

        inline void writeIfChanged(const T & val){
            if(val != data){
                write(val);
            }
        }

        inline bool isDirty() const{
            return dirty;
        }

        inline void clearFlag(){
            dirty = false;
        }

        /// @note do not save the reference!!This may cause dirtywrapper goes wrong!
        /// @note suggested using read() and write()
        inline T& get(){
            return data;
        }

        inline DirtyWrapper& operator=(const T & t){
            write(t);
            return *this;
        }
    };

    // @author:里挥发
    // @brief:事件
    class Event {
        public:
        int64_t startOn;
        int64_t interval = 0;
        std::function<void()> task;
    };

    /// @author:里挥发
    /// @brief:事件循环器
    class EventLoop {
        public:
        alib::g3::Clock clock;
        alib::g3::RateLimiter ratelimiter = alib::g3::RateLimiter(1000.0f);
        std::vector<Event> events;
        bool isRunning;

        inline void setInterval(const std::function<void()>&task, int64_t time){
            int64_t now = (int64_t) clock.getAllTime();
            Event event;
            event.startOn = now + time;
            event.task = task;
            event.interval = time;
            addEvent(event);
        }

        inline void setTimeout(const std::function<void()>& task, int64_t time){
            int64_t now = (int64_t) clock.getAllTime();
            Event event;
            event.startOn = now + time;
            event.task = task;
            addEvent(event);
        }

        inline void addEvent(Event& event){
            events.push_back(event);
        }

        inline void loop(){
            while (isRunning){
                std::vector<int> toErase;
                // 事件循环
                int size = events.size();
                for (int index = 0;index < size;index++){
                    auto& event = events[index];
                    int64_t now = (int64_t) clock.getAllTime();
                    if (now >= event.startOn){
                        event.task();
                        if (!isRunning) break;
                        if (event.interval > 0){
                            event.startOn = now + event.interval;
                        }else {
                            toErase.push_back(index);
                        }
                    }
                }
                // 删除事件
                for (int& i : toErase){
                    events.erase(events.begin() + i);
                }
                ratelimiter.wait();
            }
        }

        inline void start(){
            isRunning = true;
            loop();
        }

        inline void stop(){
            isRunning = false;
        }
    };
}
#endif
