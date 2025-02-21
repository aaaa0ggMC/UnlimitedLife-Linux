///since g3:Only utf-8 version is supported!
#ifndef ATRANSLATOR_H_INCLUDED
#define ATRANSLATOR_H_INCLUDED

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <stdarg.h>
#include <alib-g3/autil.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define ALIB_TRANSLATE_BUFFER_INITIAL_SIZE 65535
#define DEFAULT_KEY "en_us"

#define ALIB_TRANSLATION_MISSING_DEFAULT -10000
#define ALIB_TRANSLATION_MISSING_EXPECTED -10001
#define ALIB_TRANSLATION_MISSING_TRANSLATOR -10002
#define ALIB_TRANSLATION_MISSING_KEY -10003
#define ALIB_TRANSLATION_MISSING_INSTANCE -10004
#define ALIB_TRANSLATION_VERIFY_TOKEN_NOT_SET -10005
#define ALIB_TRANSLATION_ACCESS_TOKEN_NOT_SET -10005

#define ALIB_DEF_VERIFY "Language"
#define ALIB_DEF_ACCESS "Access"

///since generation3,only utf8 is guaranteed to work well

namespace alib{
namespace g3{
    ///文字多语言支持,目前支持json与toml文件的读取
    class DLL_EXPORT Translator{
    private:
        static Translator * instance;
        std::string strBuffer;
    public:
        using TransMap = std::unordered_map<std::string,std::string>;

        TransMap* currentTranslation;
        std::string verifyToken;
        std::string accessToken;
        std::unordered_map<std::string,TransMap> translations;
        std::string defaultKey;

        Translator(dstring defKey,dstring verify = ALIB_DEF_VERIFY,dstring access = ALIB_DEF_ACCESS,bool setInsanceIfNULL = true);

        const std::string& translate(dstring id);

        ///placeHolder:it's reserved,any value is ok,used for va_list
        std::string& translate_args(dstring id,string & appender,int placeHolder, ...);
        int translate_args_vlist(dstring id,string & appender,va_list);

        int loadTranslation(dstring language_id);

        int readTranslationFiles(dstring path);

        void setDefaultKey(dstring s);
        void setVerifyToken(dstring verifyUTF8);
        void setAccessToken(dstring accessUTF8);

        const std::string& getVerifyToken();
        const std::string& getAccessToken();
        const std::string& getDefaultKey();

        dstring translate_def(dstring id,dstring def);

        ///placeHolder:it's reserved,any value is ok,used for va_list
        std::string& translate_args_def(dstring id,dstring def,std::string & appender,int placeHolder, ...);

        void translate_args_internal(dstring u8_str, std::string& u8s, va_list va);

        static void set(Translator *);
        static Translator* get();
    };

}
}

#ifdef __cplusplus
}
#endif

#endif // ATRANSLATOR_H_INCLUDED
