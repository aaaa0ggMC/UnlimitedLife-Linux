#include <alib-g3/adata.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/allocators.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <toml.hpp>

using namespace alib::g3;
using namespace parsers;

void parse_node_recursive(Document::Node& parent_node, const rapidjson::Value& value){
    if (value.IsObject()) {
        for (auto it = value.MemberBegin(); it != value.MemberEnd(); ++it) {
            // 有名节点：存入 node_mapping
            std::string_view key(it->name.GetString(), it->name.GetStringLength());
            if(key == ""){
                // 作为空的key，我这里进行特殊处理，
            }
            
            auto child_wrapper = parent_node.emplace_child(key);
            parse_node_recursive(parent_node.nodes[child_wrapper.index], it->value);
        }
    } 
    else if (value.IsArray()) {
        for (rapidjson::SizeType i = 0; i < value.Size(); ++i) {
            // 匿名节点：传入空字符串，emplace_child 会自动将其物理索引存入 array_part
            auto child_wrapper = parent_node.emplace_child(""); 
            parse_node_recursive(parent_node.nodes[child_wrapper.index], value[i]);
        }
    } 
    else if (value.IsString()) {
        parent_node.set_value(std::string_view(value.GetString(), value.GetStringLength()));
    } 
    else if (value.IsInt()) {
        parent_node.set_value(to_string_optimized(value.GetInt()));
    } 
    else if (value.IsInt64()) {
        parent_node.set_value(to_string_optimized(value.GetInt64()));
    }
    else if (value.IsDouble()) {
        parent_node.set_value(ext_toString::toString(value.GetDouble()));
    } 
    else if (value.IsBool()) {
        parent_node.set_value(value.GetBool() ? "1" : "0");
    }
    else if (value.IsNull()) {
        parent_node.set_value(""); // 或者定义一个特定的 NULL 常量
    }
}

int JSON::parse(Document::Node& root, std::string_view data){
    rapidjson::Document document;
    document.Parse(data.data(), data.size());
    if(document.HasParseError()){
        return static_cast<int>(document.GetParseError());
    }
    
    std::deque<decltype(document.MemberBegin())> value_stack;
    std::deque<Document::Node*> nodes;
    value_stack.insert(value_stack.end(),document.MemberBegin(),document.MemberEnd());

    nodes.push_back(&root);
    while(!value_stack.empty()){
        auto it = value_stack.back();
        auto& node = *nodes.back();
        value_stack.pop_back();

        auto key = std::string_view(it->name.GetString(),it->name.GetStringLength());
        auto & val = it->value;
        
        if(key == ""){
            // 数组成分
            auto & tg = val.GetArray()[0];
        }else{
            // 正常子类
            auto new_node = node.emplace_child();
            nodes.push_back(new_node.ptr());

            if(val.IsObject()){
                
                // 这个会牵涉到子node，因此需要不断进行处理
                continue;
            }else if(val.IsArray()){


            }
        }
        nodes.pop_back();
    }


    return 0;
}

RefWrapper<Document::Node::nodes_t> Document::Node::get_child(std::string_view name){
    auto it = node_mapping.find(name);
    return (it != node_mapping.end()) ? ref(nodes,it->second) : RefWrapper<nodes_t>();
}

void Document::Node::set_value(std::string_view data){
    string_value = data;
}

void Document::Node::del_data(size_t index){
    [[unlikely]] panic_if(index >= nodes.size(), "Array out of bounds!");
    decltype(node_mapping)::iterator erase_it = node_mapping.end();
    decltype(array_part)::iterator erase_ait = array_part.end();
    for(auto it = node_mapping.begin(); it != node_mapping.end(); ++it){
        if(it->second > index){
            it->second -= 1;
        }else if(it->second == index){
            erase_it = it;
        }
    }
    for(auto it = array_part.begin();it < array_part.end();++it){
        if(*it > index){
            *it -= 1;
        }else if(*it == index){
            erase_ait = it;
        }
    }
    nodes.erase(nodes.begin() + index);
    if(erase_it != node_mapping.end())node_mapping.erase(erase_it);
    else if(erase_ait != array_part.end())array_part.erase(erase_ait);
}

RefWrapper<Document::Node::nodes_t> Document::Node::emplace_child(std::string_view name){
    if(name != ""){
        auto it = node_mapping.find(name);
        if(it != node_mapping.end())return ref(nodes,it->second);
    }

    auto res = nodes.get_allocator().resource();
    auto& n = nodes.emplace_back(*allocator, res, node_mapping.get_allocator().resource(), document);
    n.node_name = document->__push_constant_string(name);
    
    auto reference = ref(nodes, nodes.size() - 1);
    if(name != ""){
        node_mapping.emplace(n.node_name, nodes.size()-1);
    }else{
        array_part.push_back(nodes.size()-1);
    }
    return reference;
}

void Document::Node::del_child(std::string_view name){
    auto it = node_mapping.find(name);
    if(it != node_mapping.end()) del_data(it->second);
}

// Document Implementation
Document::Document()
:main_res(), str_res(), 
    str_pool(0, std::hash<std::pmr::string>{}, std::equal_to<std::pmr::string>{}, &str_res),
    value_cache(0, &main_res, &main_res),
    root(value_cache, &main_res, &main_res, this){}

std::string_view Document::__push_constant_string(std::string_view str){
    auto [it, inserted] = str_pool.emplace(std::pmr::string(str, &str_res));
    return *it;
}

void Document::clear(){
    root.~Node();
    str_pool.~unordered_set();
    str_res.release();
    value_cache.clear();
    
    new (&str_pool) std::pmr::unordered_set<std::pmr::string>{
        0, std::hash<std::pmr::string>{}, std::equal_to<std::pmr::string>{}, &str_res
    };
    new (&root) Node(value_cache, &main_res, &main_res, this);
}

Document::Node::Node(value_cache_t& alloc,
    std::pmr::memory_resource* node_alloc,
    std::pmr::memory_resource* mapping_alloc,
    Document* doc
):allocator(&alloc), document(doc), nodes(node_alloc), node_mapping(mapping_alloc){}
