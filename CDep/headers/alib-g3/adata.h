/** @file adata.h
* @brief 统一的数据处理形式
* @author aaaa0ggmc
* @date 2026/01/20
* @version 4.0
* @copyright Copyright(C)2025
*/
#ifndef ADATA_BRANDNEW_H
#define ADATA_BRANDNEW_H
#include <alib-g3/ecs/linear_storage.h>
#include <alib-g3/autil.h>
#include <alib-g3/adebug.h>
#include <alib-g3/aref.h>
#include <unordered_map>
#include <vector>
#include <unordered_set>
#include <memory_resource>
#include <charconv>
#include <format>
#include <stack>

namespace alib::g3{
    /// @brief 是否走string途径
    template<class T>
    concept IsStringLike = std::convertible_to<T, std::string_view>;


    /// @brief 由于Config名字被DSL占用了，目前选择更加广义的名字
    struct DLL_EXPORT Document{
        
        /// @brief 数据缓存区域
        struct DLL_EXPORT ValueCache{
            enum Focus{
                INT,
                DOUBLE,
                STRING
            };
            Focus currently_focused{STRING};
            int64_t int_val;
            long double double_val;

            template<class T> 
            auto& peek(std::pmr::string& current_value, int& cast_result);
        };

        using value_cache_t = ecs::detail::LinearStorage<ValueCache>;
        using ref_value_cache_t = RefWrapper<value_cache_t>;
        
        /// @brief 数据存储区域
        struct DLL_EXPORT Node{
            using nodes_t = std::pmr::vector<Node>;
            friend class Document;
        private:
            std::string_view node_name {""};
            value_cache_t* allocator;
            Document* document{nullptr};
            std::pmr::string string_value;
            ref_value_cache_t cache;

            void del_data(size_t target_pos);
        public:
            std::pmr::vector<Node> nodes;
            std::pmr::vector<size_t> array_part;
            std::pmr::unordered_map<std::string_view, size_t> node_mapping;

            Node(value_cache_t& alloc,
                std::pmr::memory_resource* node_alloc,
                std::pmr::memory_resource* mapping_alloc,
                Document* doc
            );

            template<class T = const char *> auto& peek();

            RefWrapper<nodes_t> get_child(std::string_view name);
            RefWrapper<nodes_t> emplace_child(std::string_view name = "");
            void del_child(std::string_view name);
            void set_value(std::string_view value);

            Node& operator[](size_t index) {
                [[unlikely]] panic_if(index >= array_part.size(), "Array index out of bounds!");
                return nodes[array_part[index]]; 
            }

            Node& operator[](std::string_view name) {
                auto res = get_child(name);
                [[unlikely]] panic_if(!res.valid(), "Child node not found!");
                return nodes[res.index];
            }
        };

        /// @warn 按照初始化顺序定义元素！！
        std::pmr::synchronized_pool_resource main_res;
        std::pmr::monotonic_buffer_resource str_res;
        /// @brief 字符串常量池
        std::pmr::unordered_set<std::pmr::string> str_pool;
        /// @brief 数据缓存
        value_cache_t value_cache;
        /// @brief 根节点
        Node root;

        /// 构造Doc对象
        Document();
        /// @brief 清除当前所有的数据
        void clear();
        
        /// @brief 添加一个新的常量字符串
        std::string_view __push_constant_string(std::string_view str);
        
        /// @brief 加载数据
        template<class T> int load_from_mem(std::string_view data,T && parser = T());
        /// @brief 从文件中加载
        template<class T> int load_from_file(std::string_view filep,T&& parser = T()){
            std::string data = "";
            // 目前还是兼容一下吧
            Util::io_readAll(std::string(filep),data);
            return load_from_mem<T>(data,std::forward<T>(parser));
        }

        inline Node& operator[](size_t index) {
            return root[index];       
        }

        inline Node& operator[](std::string_view name) {
            return root[name];
        }
    };

    /// @brief 合格的转化器
    template<class T>
    concept IsParser = requires(T&t,Document::Node & root,std::string_view data){
        { t.parse(root,data) } -> std::convertible_to<int>;
    };

    /// 一些常见的Parser
    namespace parsers{
        struct DLL_EXPORT JSON{
            int parse(Document::Node & root,std::string_view data);
        };
        struct DLL_EXPORT TOML{
            int parse(Document::Node & root,std::string_view data);
        };
        struct DLL_EXPORT ACONF{
            int parse(Document::Node & root,std::string_view data);
        };
    };

    struct RuleDocument{
        Document doc;

        inline int load_from_mem(std::string_view json_data){
            return doc.load_from_mem<parsers::JSON>(json_data);
        }

        inline int load_from_file(std::string_view json_filep){
            return doc.load_from_file<parsers::JSON>(json_filep);
        }
    };

    template<class T> 
    inline int Document::load_from_mem(std::string_view data,T && t){
        static_assert(IsParser<T>,"Current type is not a valid parser!");
        return t.parse(root,data);
    }

    template<class T> 
    inline auto& Document::ValueCache::peek(std::pmr::string& current_value, int& cast_result){
        if constexpr(IsStringLike<T>){
            switch(currently_focused){
            case STRING:
                return current_value;
            case INT:
                current_value.clear();
                std::format_to(std::back_inserter(current_value), "{}", int_val);
                break;
            case DOUBLE:
                current_value.clear();
                std::format_to(std::back_inserter(current_value), "{}", double_val);
                break;
            }
            currently_focused = STRING;
            return current_value;
        }else if constexpr(std::is_integral_v<T>){
            switch(currently_focused){
            case INT:
                return int_val;
            case STRING:{
                const char* last = current_value.c_str() + current_value.size() - 1;
                auto result = std::from_chars(current_value.c_str(), last + 1, int_val, 10);
                if(result.ptr != last + 1 && (*result.ptr != '.')){
                    cast_result = result.ptr - current_value.c_str() + 1;
                }
                break;
            }
            case DOUBLE:
                int_val = (int64_t)double_val;
                break;
            }
            currently_focused = INT;
            return int_val;
        }else if constexpr(std::is_floating_point_v<T>){
            switch(currently_focused){
            case DOUBLE:
                return double_val;
            case STRING:{
                char* flag = nullptr;
                double_val = strtold(current_value.c_str(), &flag);
                if(flag != (current_value.c_str() + current_value.size())){
                    cast_result = flag - current_value.c_str() + 1;
                }
                break;
            }
            case INT:
                double_val = (long double)int_val;
                break;
            }
            currently_focused = DOUBLE;
            return double_val;
        }else{
            static_assert(std::is_floating_point_v<T>, "You have passed unsupported types!");
        }
    }

    template<class T>
    inline auto& Document::Node::peek(){
        int ecode = 0;
        if constexpr(IsStringLike<T>){
            if(cache.valid()){
                auto & r = cache->peek<T>(string_value, ecode);
                panicf_debug(ecode, "Error occurred!With code {}",ecode);
                return r;
            }
            else return string_value;
        }else{
            if(!cache.valid()){
                bool flag;
                size_t index;
                allocator->try_next_with_index(flag, index);
                cache = ref(*allocator, index);
            }
            auto & r = cache->peek<T>(string_value, ecode);
            panicf_debug(ecode, "Error occurred!With code {}",ecode);
            return r;
        }
    }
}

#endif