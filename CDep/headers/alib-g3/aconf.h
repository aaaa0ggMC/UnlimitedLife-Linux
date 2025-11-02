/** @file aconf.h
* @brief 一个简易的DSL，可用于配置文件
* @author aaaa0ggmc
* @date 2025-11-02
* @version 3.5
* @copyright Copyright(C)2025
@par 配置文件语法：
以分号作为分割符，数据空格间隔开直接平铺写下去，可以使用""进行包装，例子：

header "你好 世界！" "fu\n" { 
    child0 "天";
} {
    child1 "地"
} 有点怪;

解析结果为:
header
-- [array]:{"你好 世界！","fu\n","有点怪"}
-- child0
---- [array]:{"天"}
-- child1
----[array]:{"地"}

可以嵌套{},然后就没了
另外，你无法给root节点添加array item.
```nginx
item 1 1 1 1;
item2 1 1 1 1;
```
这两个最终是 [root.(这个没必要写，这里是形象化)]item 和 [root.]item2 而不是root的array中的项目
重复写item可以合并内容
*/
#ifndef ACONF_H_INCLUDED
#define ACONF_H_INCLUDED
#include <alib-g3/aconf_det.h>
#include <alib-g3/autil.h>
#include <span>
#include <ranges>
#include <string.h>
#include <stdlib.h>
#include <stack>

namespace alib::g3{
    /**
     * @brief 处理的结果
     * @start-date 2025/11/02
     */
    struct DLL_EXPORT ConfigLoadResult{
        enum Code : uint32_t{
            OK = 0, ///< 没问题
            EOFTooEarly, ///< 传来的string是空的，词法分析未过
            /// 类似这样 "{ ... } ...;" 你的node还没有名字就插入block了，正确的应该是"name {...} ..."
            EmptyNodeName, 
            /// 太多 } ; 之类的使得当前的分析树被清空无法进行下一步分析
            TooManyBraces,
            /// " 未终止，因为我们的词法分析器是允许多行的，因此只会在词法分析快做完时发现问题，词法分析未通过
            UnclosedString
        };

        Code code; ///< 错误码
        int line; ///< 出错大致行号，除了unclosed string,一般来讲偏差不大
        int col; ///< 列号，不方便计算，目前恒为0
        
        /// constructor
        ConfigLoadResult(Code c,int cline = 0,int ccol = 0){
            code = c;
            line = cline;
            col = ccol;
        }
    };

    /**
     * @brief 简易DSL核心
     * @start-date 2025/11/02
     */
    struct DLL_EXPORT Config{
        /**
         * @brief 每个节点
         * @start-date 2025/11/02
         */
        struct Node{
            /// 节点的名字
            std::pmr::string name;
            /// 节点的数组值列表
            std::vector<std::pmr::string> values;
            /// 节点的字节点
            std::vector<Node> children;

            /**
             * @brief 递归获取对应的node
             * @param location 具体的递归链条
             * @param expected_index 期望第几个，默认第一个，要是期望值大于实际值返回实际值的最大值
             * @return Node的引用，会在一个都没找到时返回std::nullopt
             * @start-date 2025/11/02
             */
            std::optional<std::reference_wrapper<const Config::Node>> 
                get_node_recursive(const std::vector<std::string_view> & location,size_t expected_index = 0) const;

            /**
             * @brief 获取子节点中所有名字为name的节点
             * @param name 名字
             * @return 节点的ranges view,采用惰性计算，拿来遍历刚刚好
             * @start-date 2025/11/02
             */
            inline auto get_child_nodes_view(const std::string& name) const{
                return children | std::views::filter([name](const Node & node){
                    return !strcmp(node.name.c_str(),name.c_str());
                });
            }

            /**
             * @brief 递归获取对应节点的对应数值
             * @param location 见 Node::get_node_recursive
             * @param expected_index 见 Node::get_node_recursive
             * @param value_index 索引
             * @return 字符串数据的view,没有对应索引或者一个node都找不到返回std::nullopt 
             * @start-date 2025/11/02
             */
            inline std::optional<std::string_view> 
                get_node_recursive_value(const std::vector<std::string_view> & location,size_t expected_index = 0,size_t value_index = 0) const{
                auto node = get_node_recursive(location,expected_index);
                if(node && node->get().values.size() > value_index){
                    return node->get().values[value_index];
                }else return std::nullopt;
            }

            /**
             * @brief 获取数组值
             * @param value_index 索引
             * @return 没有对应索引返回std::nullopt
             * @start-date 2025/11/02
             */
            inline std::optional<std::string_view> get_value(size_t value_index) const{
                if(value_index >= values.size())return std::nullopt;
                return values[value_index];
            }

            /**
             * @brief 用于测试，打印节点
             * @param node 根节点
             * @param indent 缩进
             * @start-date 2025/11/02
             */
            static void print_node(const Config::Node& node, int indent = 0);
            /**
             * @brief 用于测试，打印自身
             * @param indent 缩进
             * @start-date 2025/11/02
             */
            inline void print_node(int indent = 0) const{print_node(*this,indent);}

            /**
             * @brief 获取转换后的数值
             * @tparam T 类型
             * @tparam CastFn 转换函数，默认支持int long short bool float double，要求有try_cast(这里没写concept了，算是ducking type) 
             * @param value_index 同其他的
             * @return 返回转换结果，nullopt表示转换失败，当不为nullopt但是errpos != -1的时候说明出现了小错误， \n 
             *      比如 1123nnn (to int)1123表示后面没转换完（当且仅当CastFn默认）
             * @start-date 2025/11/02
             */
            template<class T,class CastFn = DefConfigCaster> inline ConfigCastResult<T>  
                get_value_as(size_t value_index) 
            const{
                auto v = get_value(value_index);
                if(v){
                    return CastFn::template try_cast<T>(*v);
                }else return ConfigCastResult<T>{std::nullopt,-1};
            }

            /**
             * @brief 获取转换后的数值
             * @tparam T 类型
             * @tparam CastFn 转换函数，默认支持int long short bool float double，要求有try_cast(这里没写concept了，算是ducking type)
             * @param value_index 同其他的
             * @return 返回转换结果，nullopt表示转换失败，当不为nullopt但是errpos != -1的时候说明出现了小错误， \n 
             *      比如 1123nnn (to int)1123表示后面没转换完（当且仅当CastFn默认）
             * @start-date 2025/11/02
             */
            template<class T,class CastFn = DefConfigCaster> inline ConfigCastResult<T> 
                get_node_recursive_value_as(
                    const std::vector<std::string_view> & location,
                    size_t expected_index = 0,
                    size_t value_index = 0)
            const{
                auto v = get_node_recursive_value(location,expected_index,value_index);
                if(v){
                    return CastFn::template try_cast<T>(*v);
                }else return ConfigCastResult<T>{std::nullopt,-1};
            }

            template<CanDump Dumper> inline void dump(Dumper & appender,size_t indent = 4){
                using namespace detail;
                std::string indent_str(indent,' ');
                std::stack<Node*> tree;
                int depth = 0;
                if(this->name.empty()){ // 说明是匿名节点，一般一定为root，一定无value，进行一层遍历
                    for(auto & node : this->children){
                        tree.push(&node);
                    }
                }else tree.push(this);

                static auto add_str = [](Dumper & dmp,std::string_view sv){
                    dump_append(dmp,"\"");
                    for(auto ch : sv){
                        if(ch == '\n'){
                            dump_append(dmp,"\\n");
                        }else if(ch == '\t'){
                            dump_append(dmp,"\\t");
                        }else if(ch == '\\'){
                            dump_append(dmp,"\\\\");
                        }else if(ch == '\"'){
                            dump_append(dmp,"\\\"");
                        }else if(ch =='\''){
                            dump_append(dmp,"\\\'");
                        }else dump_append(dmp,std::string_view(&ch,1));
                    }
                    dump_append(dmp,"\"");
                };

                while(!tree.empty()){
                    Node * top = tree.top();

                    ///这里缩进
                    for(int i = 0;i < depth;++i){
                        dump_append(appender,indent_str);
                    }

                    add_str(appender,top->name);

                    for(auto & value : top->values){
                        dump_append(appender," ");
                        add_str(appender,value);
                    }
                    dump_append(appender," ");

                    // Add children
                    if(top->children.size()){
                        dump_append(appender,"{\n");
                        for(auto & node : top->children)tree.push(&node);
                        ++depth;
                        continue; // 说明目前还是父节点
                    }

                    tree.pop(); // 目前他没有孩子了
                    if(tree.empty()){
                        dump_append(appender,";\n");
                        break;
                    }
                    Node & new_top = *tree.top();
                    if(new_top.children.size()){
                        /// 内存差
                        int off = top - &(new_top.children[0]);
                        /// 不在对应的vector数据范围内
                        if(off < 0 && off >= sizeof(Node) * new_top.children.size()){
                            /// 说明也是平级
                            dump_append(appender,";\n");
                        }else{
                            /// 父节点，开心消消乐
                            tree.pop();
                            --depth;
                            dump_append(appender,";\n");
                            for(int i = 0;i < depth;++i){
                                dump_append(appender,indent_str);
                            }
                            dump_append(appender,"};\n");
                        }
                    }else{
                        //上一个元素没有子节点，说明应该是平级
                        dump_append(appender,";\n");
                    }
                }
            }
        };

        Node root; ///< 根节点，默认为匿名

        /// 从stream中加载，不会清除当前数据
        ConfigLoadResult load(std::istream & is);
        /// 从字符串中加载，不会清除当前数据
        ConfigLoadResult load(std::string_view str);
        /// 清除当前数据
        void clear();
    private:
        /// @brief 词法分析
        /// @param tokens token表
        /// @param hints 行号提醒
        /// @return 词法分析的加载结果
        ConfigLoadResult analyse_words(std::span<std::string> tokens,std::span<int> hints);
    };
}


#endif