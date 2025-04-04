/** @file adata.h
* @brief 关于数据的类
* @author aaaa0ggmc
* @date 2025-4-04
* @version 3.1
* @copyright Copyright(C)2025
********************************
@par 修改日志:
<table>
<tr><th>时间       <th>版本         <th>作者          <th>介绍
<tr><td>2025-4-04 <td>3.1          <th>aaaa0ggmc    <td>添加doc
</table>
********************************
*/
#ifndef ADATA_H_INCLUDED
#define ADATA_H_INCLUDED
#include <string>
#include <unordered_map>
#include <optional>
#include <stdarg.h>
#include <alib-g3/autil.h>

///出现了解析错误
#define AE_HAS_PARSE_ERROR -1000

namespace alib{
namespace g3{
    using mapping_tp = std::unordered_map<std::string,std::string>;

    /** @struct Analyser
     * @brief 分析器，一般不需要管
     */
    class DLL_EXPORT Analyser{
    public:
        mapping_tp & mapping;///<kv映射
        ///解析字符串
        virtual int parseString(dstring data);
        ///处理映射
        virtual void mapDocument();
        ///获取const值
        virtual dstring getConst(dstring key);
        ///获取一个copy
        std::string getCopy(dstring key);

        virtual ~Analyser();
        Analyser(mapping_tp & mtp);

        const static std::string empty_ret;///<返回空值
    };

    /** @struct GDoc
     *  @brief GenericDocument，通用的文档处理器，支持toml与json(json有点小问题)
     */
    class DLL_EXPORT GDoc{
    public:

        Analyser * analyser;///<分析器
        mapping_tp mapping;///<映射

        GDoc();
        ~GDoc();

        int read_parseFileJSON(dstring fpath);
        int read_parseStringJSON(dstring data);


        int read_parseFileTOML(dstring fpath);
        int read_parseStringTOML(dstring data);

        void clearMapping();

        std::optional<const char *> get(dstring key);
        template<class... T> inline std::optional<const char *> tget(const T&... args){
            std::string keyfn;
            keyfn.reserve(128);
            ((keyfn += std::string(args) + "."), ...);
            if(keyfn.length() >= 1)keyfn.erase(keyfn.length() - 1);
            return get(keyfn);
        }

        std::optional<const char*> operator [](dstring key);

    };
}
}


#endif // ADATA_H_INCLUDED
