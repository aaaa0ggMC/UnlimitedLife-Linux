/** @file atranslator.h
 * @brief 翻译器
 * @author aaaa0ggmc
 * @date 2025-6-11
 * @version 3.1
 * @copyright copyright(C)2025
 ********************************
 @ *par 修改日志:                      *
 <table>
 <tr><th>时间       <th>版本         <th>作者          <th>介绍
 <tr><td>2025-6-11 <td>3.1          <th>aaaa0ggmc    <td>添加doc
 <tr><td>2025-6-11 <td>3.1          <th>aaaa0ggmc    <td>完成doc
 </table>
 ********************************
 */
//since g3:Only utf-8 version is supported!
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
///多参数翻译的初始buffer大小
#define ALIB_TRANSLATE_BUFFER_INITIAL_SIZE 4096
///默认的语言key值
#define DEFAULT_KEY "en_us"

//errors
///翻译表中中不存在默认翻译
#define ALIB_TRANSLATION_MISSING_DEFAULT -10000
///翻译表中不存在期望的语言
#define ALIB_TRANSLATION_MISSING_EXPECTED -10001
///目前的translator不存在(currentTranslation == NULL)，需要指定
#define ALIB_TRANSLATION_MISSING_TRANSLATOR -10002
///没有所谓的key
#define ALIB_TRANSLATION_MISSING_KEY -10003
///没有静态的instance
#define ALIB_TRANSLATION_MISSING_INSTANCE -10004
///没设置VerifyToken
#define ALIB_TRANSLATION_VERIFY_TOKEN_NOT_SET -10005
///没设置AccessToken
#define ALIB_TRANSLATION_ACCESS_TOKEN_NOT_SET -10005
///默认Verify
#define ALIB_DEF_VERIFY "Language"
///默认Access
#define ALIB_DEF_ACCESS "Access"

//since generation3,only utf8 is guaranteed to work well
namespace alib{
namespace g3{
    /** @struct Translator
     * @brief 文字多语言支持,目前支持json与toml文件的读取
     * @todo lazyloading..(也许也没那么必要)
     */
    class DLL_EXPORT Translator{
    private:
        ///静态实例
        static Translator * instance;
        ///内部的字符串缓冲
        std::string strBuffer;
    public:
        using TransMap = std::unordered_map<std::string,std::string>;
        ///目前的翻译
        TransMap* currentTranslation;
        std::string verifyToken; /// 标示token,用于指定翻译文件是否有效
        std::string accessToken; /// 访问token,用于程序访问名字
        std::unordered_map<std::string,TransMap> translations; /// 存储表
        std::string defaultKey; /// 默认的语言key值

        /// 构造函数
        Translator(dstring defKey,dstring verify = ALIB_DEF_VERIFY,dstring access = ALIB_DEF_ACCESS,bool setInsanceIfNULL = true);

        /// 简单地通过id翻译 @return 返回transMap中的引用
        const std::string& translate(dstring id);

        /** @brief 带参数的翻译
         *  @param[in] id id
         *  @param[out] appender 追加到末尾
         *  @param[in] placeHolder 保留，用于va_list定位，随便一个值都可以
         *  ... va_args
         *  @return 返回strBuffer,所以线程不安全，一般建议copy一遍
         */
        std::string& translate_args(dstring id,std::string & appender,int placeHolder, ...);

        /** @brief 内部实现
          * @return ALIB_TRANSLATION_MISSING_TRANSLATOR | ALIB_TRANSLATION_MISSING_KEY | ALIB_SUCCESS
          */
        int translate_args_vlist(dstring id,std::string & appender,va_list);

        /** @brief 加载翻译
         * @return ALIB_TRANSLATION_MISSING_DEFAULT | ALIB_TRANSLATION_MISSING_EXPECTED | AE_SUCCESS
         */
        int loadTranslation(dstring language_id);

        /** @brief 加载翻译文件
         * @return ALIB_TRANSLATION_VERIFY_TOKEN_NOT_SET | ALIB_TRANSLATION_ACCESS_TOKEN_NOT_SET | AE_SUCCESS
         */
        int readTranslationFiles(dstring path);

        /// 设置默认的key
        Translator& setDefaultKey(dstring s);
        /// 设置默认VerifyToken
        Translator& setVerifyToken(dstring verifyUTF8);
        /// 设置默认AccessToken
        Translator& setAccessToken(dstring accessUTF8);

        /// 获取当前vtoken
        const std::string& getVerifyToken();
        /// 获取当前atoken
        const std::string& getAccessToken();
        /// 得到默认key值
        const std::string& getDefaultKey();

        /// 带默认值的translate
        dstring translate_def(dstring id,dstring def);

        /// 带默认值的translate_args
        std::string& translate_args_def(dstring id,dstring def,std::string & appender,int placeHolder, ...);

        /// 内部vlist翻译
        void translate_args_internal(dstring u8_str, std::string& u8s, va_list va);

        /// 设置静态的translator
        static void set(Translator *);
        /// 获得当前的translator
        static std::optional<Translator> get();
    };

}
}

#ifdef __cplusplus
}
#endif

#endif // ATRANSLATOR_H_INCLUDED
