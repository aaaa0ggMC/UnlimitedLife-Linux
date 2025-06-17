#ifndef AGE_BASE
#define AGE_BASE

#include <cstdint>
#include <string>
#include <vector>
#include <memory_resource>
#include <numbers>

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

    struct DirtyMarker{
    private:
        bool dirty {true};
    public:
        inline void dm_mark(){
            dirty = true;
        }

        inline bool dm_check(){
            return dirty;
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
}

#endif
