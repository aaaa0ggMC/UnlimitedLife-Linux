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
//#include <iostream>

///对象如VAO,VBO为空
#define AGE_NULL_OBJ 0

///For windows support
#ifdef _WIN32
#include <windows.h>
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

#define AGEE_CONFLICT_SID -1

namespace age{
    inline constexpr bool AGE_Enable = true;
    inline constexpr bool AGE_Disable = false;

    template<class T> inline constexpr T rad2deg(const T& in){
        return in * (T(180) / std::numbers::pi);
    }

    template<class T> inline constexpr T deg2rad(const T & in){
        return in * (std::numbers::pi / (T)180);
    }

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

    struct AGE_API ErrorInfo{
        int32_t code;
        const char * message;
    };

    struct AGE_API ErrorInfopp{
        int32_t code;
        std::pmr::string message;
    };
    typedef void(*TriggerFunc)(const ErrorInfopp&);

    /** @struct Error
     *  @brief handle errors during many operations
     */
    struct AGE_API Error{
    public:
        TriggerFunc trigger;
        int32_t limit;

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

    // @author:里挥发
    // @brief:事件
    class Event {
        public:
        int64_t startOn;
        int64_t interval = 0;
        std::function<void()> task;
    };

    // @author:里挥发
    // @brief:事件循环器
    class EventLoop {
        public:
        alib::g3::Clock clock;
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
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
