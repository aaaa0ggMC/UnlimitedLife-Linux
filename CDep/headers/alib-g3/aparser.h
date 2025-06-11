/** @file aparser.h
 * @brief 简单的命令行解析
 * @author aaaa0ggmc
 * @date 2025-6-11
 * @version 3.1
 * @copyright copyright(C)2025
 ********************************
 @ par 修改日志:                      *
 <table>
 <tr><th>时间       <th>版本         <th>作者          <th>介绍
 <tr><td>2025-6-11 <td>3.1          <th>aaaa0ggmc    <td>添加doc
 <tr><td>2025-6-11 <td>3.1          <th>aaaa0ggmc    <td>完成doc
 </table>
 ********************************
 */
#ifndef PARSER_H_INCLUDED
#define PARSER_H_INCLUDED
#include <string>
#include <vector>
#include <alib-g3/autil.h>
#include <alib-g3/alogger.h>

namespace alib{
namespace g3{
    using dstring = const std::string &;
    template<class T> using dvector = const std::vector<T> &;
    using dsvector = const std::vector<std::string> &;

    /** @struct Parser
     *  @brief 简单的解析器
     */
    class DLL_EXPORT Parser{
    public:
        /** @brief 解析命令
         * @param[in] cmd 命令
         * @param[out] head 头
         * @param[out] args 整体的参数
         * @param[out] sep_args 分立参数
         * @par 命令规则
         * - 使用 {} 包裹一个完整的块，如 "{hello 123} {123}" ==> "hello 123" 与 "123"
         * - 所以需要嵌套才能用{,},如  "hello {123 {} }" ==> "hello" "123 {} "
         */
        int ParseCommand(dstring cmd,std::string & head,std::string& args,std::vector<std::string> & sep_args);
        ///生成参数，内部使用，从beg开始
        int gen_arg(dstring str,unsigned int beg,std::string & arg);
    };
}
}
#endif // PARSER_H_INCLUDED
