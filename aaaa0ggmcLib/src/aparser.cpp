#include <alib-g3/aparser.h>
#include <ctype.h>
#include <alib-g3/autil.h>

using namespace alib::g3;

int Parser::ParseCommand(dstring cmd,std::string & head,std::string& args,std::vector<std::string> & sep_args){
    head = "";
    args = "";
    sep_args.clear();
    unsigned int index = 0;
    std::string arg;

    index = gen_arg(cmd,index,head);
    Util::str_trim_nrt(head);

    while(index < cmd.size()){
        arg = "";
        index = gen_arg(cmd,index,arg);
        Util::str_trim_nrt(arg);
        if(arg.compare("")){
            sep_args.push_back(arg);
        }
    }

    //simple compose
    for(auto & element : sep_args){
        args += element;
        args += " ";
    }

    return 0;
}

int Parser::gen_arg(dstring str,unsigned int beg,std::string & arg){
    unsigned int index = beg;
    int depth = 0;
    for(;index < str.size();){
        char ch = str[index];
        index += 1;
        switch(ch){
        case '{':
            if(depth > 0){
                arg += '{';
            }
            depth++;
            break;
        case '}':
            depth--;
            if(depth > 0){
                arg += '}';
            }
            break;
        default:
            if(depth == 0 && isspace(ch)){
                goto exit_sp;
            }
            arg += ch;
            break;
        }
        continue;
        exit_sp:
            break;
    }
    return index;
}
