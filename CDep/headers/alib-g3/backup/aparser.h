#ifndef PARSER_H_INCLUDED
#define PARSER_H_INCLUDED
#include <string>
#include <vector>

using dstring = const std::string &;
template<class T> using dvector = const std::vector<T> &;
using dsvector = const std::vector<std::string> &;

class Parser{
public:
    int ParseCommand(dstring cmd,std::string & head,std::string& args,std::vector<std::string> & sep_args);

    int gen_arg(dstring str,unsigned int beg,std::string & arg);
};

#endif // PARSER_H_INCLUDED
