#ifndef A_STRING_H_INCLUDED
#define A_STRING_H_INCLUDED
#include <string>
#include <alib-g3/autil.h>

namespace alib{
namespace ng{
    using dwstring = const std::wstring&;

    struct mstring{
        std::string ansi;
        std::string utf8;
        std::wstring utf16;
    };

    class DLL_EXPORT converter{
    public:
        static std::string utf8_to_ansi(dstring utf8);
        static std::string utf16_to_ansi(dwstring utf8);

        static std::string ansi_to_utf8(dstring ansi);
        static std::string utf16_to_utf8(dwstring utf16);

        static std::wstring ansi_to_utf16(dstring ansi);
        static std::wstring utf8_to_utf16(dstring utf8);
    };

}
}

#endif // A_STRING_H_INCLUDED
