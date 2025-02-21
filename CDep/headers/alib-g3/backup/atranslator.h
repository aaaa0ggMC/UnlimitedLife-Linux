#ifndef ATRANSLATOR_H_INCLUDED
#define ATRANSLATOR_H_INCLUDED

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

#include <stdarg.h>

#include <alib-g3/astring.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define TEXT_MAX_SIZE 65535
#define DEFAULT_KEY "en_us"

#define ALIB_ENC_UTF8 0b1
#define ALIB_ENC_ANSI 0b10
#define ALIB_ENC_UTF16 0b100

#define ALIB_TRANSLATION_MISSING_DEFAULT -10000
#define ALIB_TRANSLATION_MISSING_EXPECTED -10001
#define ALIB_TRANSLATION_MISSING_TRANSLATOR -10002
#define ALIB_TRANSLATION_MISSING_KEY -10003
#define ALIB_TRANSLATION_MISSING_INSTANCE -10004
#define ALIB_TRANSLATION_VERIFY_TOKEN_NOT_SET -10005
#define ALIB_TRANSLATION_ACCESS_TOKEN_NOT_SET -10005

#define ALIB_DEF_VERIFY "Language"
#define ALIB_DEF_ACCESS "Access"

namespace alib{
namespace ng{


    ///文字多语言支持,目前支持json与toml文件的读取
    class DLL_EXPORT Translator{
    private:
        static Translator * instance;
        std::string strBuffer;
    public:

        std::optional<std::string> verifyToken;
        std::optional<std::string> accessToken;


        using TransMap = std::unordered_map<std::string,mstring>;

        TransMap * currentTranslation;
        std::unordered_map<std::string,TransMap> translations;
        std::string defaultKey;

        Translator(bool setInstanceIfNULL = false,dstring defaultKey = DEFAULT_KEY);

        std::optional<mstring*> translate(dstring id,int enc = ALIB_ENC_UTF8);

        int translate_args(dstring id,int enc,string & storer,int encStore,...);
        int translate_args_vlist(dstring id,int enc,string & storer,int encStore,va_list);

        //utf16 only
        int translate_args(dstring id,int enc,std::wstring* storer, ...);
        int translate_args_vlist(dstring id,int enc,std::wstring* storer,va_list);

        //suggest utf8
        void setDefaultKey(dstring s);

        int loadTranslation(dstring language_id);

        int readTranslationFiles(dstring path,int enc = ALIB_ENC_UTF8);

        static std::optional<mstring*> translate_static(dstring id,int enc = ALIB_ENC_UTF8);
        static int translate_args_static(dstring id,int enc,std::string & storer,int encStore,...);
        static int translate_args_static(dstring id,int enc,std::wstring* storer,...);

        void translate_args_internal(dstring u8_str,std::string& u8s,va_list va);

        void setVerifyToken(dstring verifyUTF8);
        void setAccessToken(dstring accessUTF8);

        dstring translate_def(dstring id,dstring def,int enc=ALIB_ENC_UTF8);
        ///返回u8
        std::string translate_args_def(dstring id,dstring def,int enc,...);


        static void set(Translator *);
        static Translator * get();
    };

}
}

#ifdef __cplusplus
}
#endif

#endif // ATRANSLATOR_H_INCLUDED
