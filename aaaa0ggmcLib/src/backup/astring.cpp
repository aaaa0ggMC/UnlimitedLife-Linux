#include <alib-g3/astring.h>
#include <alib-g3/autil.h>
#include <codecvt>
#include <locale>

using namespace alib::g3;

std::string converter::utf8_to_ansi(dstring utf8) {
    return Util::str_encUTF8ToAnsi(utf8);
}

std::string converter::utf16_to_ansi(dwstring utf16) {
    return utf8_to_ansi(utf16_to_utf8(utf16));
}

std::string converter::ansi_to_utf8(dstring ansi) {
    return Util::str_encAnsiToUTF8(ansi);
}

std::string converter::utf16_to_utf8(dwstring utf16) {
    using namespace std;
    return std::wstring_convert< std::codecvt_utf8<wchar_t>, wchar_t >{}.to_bytes(utf16);
}

std::wstring converter::ansi_to_utf16(dstring ansi) {
    return utf8_to_utf16(ansi_to_utf8(ansi));
}

std::wstring converter::utf8_to_utf16(dstring utf8) {
    return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t>{}.from_bytes(utf8);
}
