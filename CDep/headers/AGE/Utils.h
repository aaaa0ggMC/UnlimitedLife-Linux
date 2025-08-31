/**
 * @file Utils.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 一些工具
 * @version 0.1
 * @date 2025/08/31
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/06/11 （左右） 
 */
#ifndef AGE_H_BASE
#define AGE_H_BASE

#include <cstdint>
#include <string>
#include <vector>
#include <iostream>
#include <memory_resource>
#include <numbers>
#include <optional>
#include <functional>
#include <alib-g3/aclock.h>
#include <thread>
#include <chrono>
#include <functional>
#include <alib-g3/alogger.h>
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

// Error Code [-1,-9999]
#define AGEE_CONFLICT_SID -1
#define AGEE_EMPTY_DATA -2
#define AGEE_WRONG_ENUM -3
#define AGEE_FEATURE_NOT_SUPPORTED -4

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
    public:
        DirtyMarker * chain {NULL};
        inline void dm_mark(){
            if(dirty)return;
            dirty = true;
            if(chain){
                chain->dm_mark();
                //std::cout << "chained" << std::endl;
            }
        }

        inline bool dm_check(){
            return dirty;
        }

        inline void dm_clear(){
            dirty = false;
        }
    };

    enum class ErrorLevel : int32_t {
        Trace = LOG_TRACE,
        Debug = LOG_DEBUG,
        Info = LOG_INFO,
        Warn = LOG_WARN,
        Error = LOG_ERROR,
        Critical = LOG_CRITI
    };

    struct AGE_API ErrorInfo{
        int32_t code;
        const char * message;
        ErrorLevel level;
    };

    struct AGE_API ErrorInfopp{
        int32_t code;
        std::pmr::string message;
        ErrorLevel level;
    };
    using TriggerFunc = std::function<void(const ErrorInfopp&)>;

    struct AGE_API BinderArray{
        std::unordered_map<intptr_t,intptr_t> bindings;
    
        template<class T> inline T* get(intptr_t data){
            auto iter = bindings.find(data);
            if(iter != bindings.end()){
                return (T*)(iter->second);
            }
            return nullptr;
        }
    };

    struct AGE_API Binder{
        BinderArray & ba;
        intptr_t addr;
        intptr_t target;

        inline Binder(BinderArray&b):ba{b}{
            addr = 0;
        }

        inline void bind(intptr_t a,intptr_t tg){
            addr = a;
            target = tg;
            ba.bindings[a] = tg;
        }

        inline void unbind(){
            if(addr){
                auto iter = ba.bindings.find(addr);
                if(iter != ba.bindings.end()){
                    ba.bindings.erase(iter);
                }
                addr = 0;
            }
        }

        inline ~Binder(){
            //自动销毁
            unbind();
        }
    };

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
    };

    struct NonCopyable{
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
    public:
        inline const T& read() const{
            return data;
        }

        inline void write(const T& val){
            dirty = true;
            data = val;
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
