#include <alib-g3/aconf.h>
#include <stack>
#include <iostream>

using namespace alib::g3;

ConfigLoadResult Config::load(std::istream & is){
    std::string data (std::istreambuf_iterator<char>(is),{});
    return load(data);
}

void Config::clear(){
    root = Node();
}

ConfigLoadResult Config::load(std::string_view data){
    if(data.empty())return ConfigLoadResult(ConfigLoadResult::EOFTooEarly);
    std::vector<std::string> tokens;
    std::vector<int> lineHints;
    // 减少copy数目，注意，请保持在buffer未进行swap时tokens不扩容
    std::string * buffer = &tokens.emplace_back("");

    bool in_str = false;
    bool escape = false;

    size_t i = 0;

    char ch;

    while(i < data.size()){
        ch = data[i];
        ++i;
        if(in_str){
            if(escape){
                escape = false;
                /// @Notice 这里加入新的转义语序需要在Node::dump里面加入对应的转换！
                switch(ch){
                case '\\':
                case '\"':
                case '\'':
                    buffer->push_back(ch);
                    break;
                case 'n':
                    buffer->push_back('\n');
                    break;
                case 't':
                    buffer->push_back('\t');
                    break;
                default:
                    buffer->push_back('\\');
                    buffer->push_back(ch);
                }
                continue;
            }else{
                if(ch == '\\'){
                    escape = true;
                    continue;
                }else if(ch == '\"'){
                    in_str = false;
                    // and we finished a token
                    // 这里选了更加经济的方案，我真的太爱偷懒了
                    // 由于转义只在""中可以使用，因此边缘测试"\"" --> "" --(strip) -> " 也是能过的
                    // 是的，我选择了后缀表达式这个更加经济的方案
                    // 目前是 "x" --> x" --(strip)-> x 
                    buffer->push_back('\"'); // to prevent "{" "}"
                    buffer = &tokens.emplace_back("");
                    continue;
                }
                if(ch == '\n'){
                    lineHints.push_back(tokens.size());
                }
                buffer->push_back(ch);
            }
        }else{
            if(ch == '\"'){
                if(!buffer->empty()){
                    // we finished a token
                    buffer = &tokens.emplace_back("");
                }
                in_str = true;
                continue;
            }
            if(isspace(ch)){
                if(ch == '\n'){
                    lineHints.push_back(tokens.size());
                }
                if(!buffer->empty()){
                    // we finished a token
                    buffer = &tokens.emplace_back("");
                }
                continue;
            }
            if(ch == '{' || ch == '}' || ch == ';'){
                // finshed the prev token and add this token along
                if(buffer->empty()){
                    buffer->push_back(ch);
                    buffer = &tokens.emplace_back("");
                    continue;
                }else{
                    // 创建新项目 + push 这样 aaa; -> aaa 和 ; 而不是aaa;
                    tokens.emplace_back("").push_back(ch);
                    buffer = &tokens.emplace_back("");
                    continue;
                }
            }
            buffer->push_back(ch);
        }
    }

    // 删除空元素
    if(tokens.size() > 0 && tokens[tokens.size()-1].empty()){
        tokens.erase(tokens.end()-1);
    }

    if(in_str){
        if(lineHints.size())return ConfigLoadResult(ConfigLoadResult::UnclosedString,lineHints.size());
        else return ConfigLoadResult(ConfigLoadResult::UnclosedString,1);
    }

    // 最后一个默认为一行
    lineHints.push_back(tokens.size() + 1);

    return analyse_words(tokens,lineHints);
}


ConfigLoadResult Config::analyse_words(std::span<std::string> tokens,std::span<int> hints){
    std::stack<Node*> tree;
    root.children.push_back(Node());
    // 这里我来阐述一下为什么这里使用指针缓存children而不会出现内存错误
    // 因为正常情况下（边缘待测试），我们的访问是线性的，也就是对于children，我们能保证其运作时未扩容
    // 注意：每次 load() 都在 root.children 末尾追加一棵新树，
    // 因此当前解析节点应指向 back()，否则多次 load 会混入同一棵树。
    Node * current = &root.children.back();

    static auto get_data = [](std::string_view in)->std::string_view {
        if(!in.empty() && in.back() == '\"')return in.substr(0,in.size()-1);
        return in;
    };

    int line_index = 0;

    tree.push(&root);
    tree.push(current);

    for(size_t i = 0;i < tokens.size();){
        if(i > hints[line_index]){
            line_index++;
        }
        
        auto & tk = tokens[i];
        ++i;
        if(!tk.compare("{")){
            if(!current->name.compare("")){
                // at least a name is required
                return ConfigLoadResult(ConfigLoadResult::EmptyNodeName,line_index+1); 
            }
            // to the next depth of node
            current->children.push_back(Node());
            tree.push(&(*(current->children.end()-1)));
            current = tree.top(); // move to child
        }else if(!tk.compare("}")){
            if(tree.empty()){
                return ConfigLoadResult(ConfigLoadResult::TooManyBraces,line_index+1);
            }
            // to the prev depth of node
            tree.pop();
            bool dec = false;
            if(!current->name.compare(""))dec = true;
            if(tree.empty())return ConfigLoadResult(ConfigLoadResult::TooManyBraces,line_index+1);
            current = tree.top();
            if(dec && current->children.size() > 0){
                current->children.erase(current->children.end()-1);
            }
            tree.pop();
            if(tree.empty())return ConfigLoadResult(ConfigLoadResult::TooManyBraces,line_index+1);
            current = tree.top();
            // move to next 
            current->children.push_back(Node());
            tree.push(&(*(current->children.end()-1)));
            current = tree.top();
        }else if(!tk.compare(";")){
            if(!current->name.empty()){
                // add new node
                tree.pop();
                if(tree.empty())return ConfigLoadResult(ConfigLoadResult::TooManyBraces,line_index+1);
                current = tree.top();
                current->children.push_back(Node());
                tree.push(&(*(current->children.end()-1)));
                current = tree.top();
            }
            continue;
        }else{
            if(current->name.empty())current->name = get_data(tk);
            else current->values.emplace_back(get_data(tk));
        }
    }

    size_t sz = root.children.size();
    if(sz && !root.children[sz-1].name.compare("")){
        root.children.erase(root.children.end() -1);
    }

    return ConfigLoadResult(ConfigLoadResult::OK);
}

std::optional<std::reference_wrapper<const Config::Node>>
    Config::Node::get_node_recursive(const std::vector<std::string_view>& path,size_t expected_index) const{
    std::vector<std::reference_wrapper<const Node>> candidates{*this};
    for (size_t depth = 0; depth < path.size(); ++depth) {
        std::vector<std::reference_wrapper<const Node>> next;

        for (auto& node : candidates) {
            for (auto& child : node.get().children) {
                if (child.name == path[depth]) {
                    next.push_back(child);
                }
            }
        }

        if (next.empty()) return std::nullopt;
        candidates = std::move(next);
    }

    if(candidates.empty())return std::nullopt;

    if (expected_index >= candidates.size()) return candidates.back();
    return candidates[expected_index];
}

void Config::Node::print_node(const Config::Node& node, int indent){
    std::string indent_str(indent * 2, ' ');

    if(!node.name.empty()){
        std::cout << indent_str << "Node: " << node.name << std::endl;
    }else{
        std::cout << indent_str << "<unnamed node>" << std::endl;
    }

    if(!node.values.empty()){
        std::cout << indent_str << "   Values:";
        for(auto const& v : node.values)
            std::cout << " " << v;
        std::cout << std::endl;
    }

    for(auto const& child : node.children){
        print_node(child, indent + 1);
    }
}