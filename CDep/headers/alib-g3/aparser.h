#ifndef PARSER_H_INCLUDED
#define PARSER_H_INCLUDED
#include "alogger.h"
#include <string>
#include <vector>
#include <alib-g3/autil.h>

namespace alib{
namespace g3{
    using dstring = const std::string &;
    template<class T> using dvector = const std::vector<T> &;
    using dsvector = const std::vector<std::string> &;

    class DLL_EXPORT Parser{
    public:
        int ParseCommand(dstring cmd,std::string & head,std::string& args,std::vector<std::string> & sep_args);

        int gen_arg(dstring str,unsigned int beg,std::string & arg);
    };
}
}
#endif // PARSER_H_INCLUDED
