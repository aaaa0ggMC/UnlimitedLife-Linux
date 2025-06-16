#ifndef AGE_BASE
#define AGE_BASE

#include <cstdint>
#include <string>
#include <vector>

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
    inline constexpr bool AGE_Negate = true;
    inline constexpr bool AGE_Apply = false;

    struct AGE_API ErrorInfo{
        int32_t code;
        const char * message;
    };

    struct AGE_API ErrorInfopp{
        int32_t code;
        std::string message;
    };
    typedef void(*TriggerFunc)(const ErrorInfopp&);

    /** @struct Error
     *  @brief handle errors during many operations
     */
    struct AGE_API Error{
    public:
        TriggerFunc trigger;
        std::vector<ErrorInfopp> infos;

        Error();

        void setTrigger(TriggerFunc = Error::defTrigger);
        void pushMessage(const ErrorInfo&);

        static void defTrigger(const ErrorInfopp&);
    };
}

#endif
