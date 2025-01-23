#ifndef ADATA_H_INCLUDED
#define ADATA_H_INCLUDED
#include <string>
#include <unordered_map>
#include <optional>
#include <stdarg.h>
#include <alib-g3/autil.h>

#define ALIB_DATAE(x) ((x) | 0x10000000)
#define ALIB_EHAS_PARSE_ERROR ALIB_DATAE(0b1)

namespace alib{
namespace ng{
    using mapping_tp = std::unordered_map<std::string,std::string>;
    class DLL_EXPORT Analyser{
    public:
        mapping_tp & mapping;
        virtual int parseString(dstring data);
        virtual void mapDocument();
        virtual dstring getConst(dstring key);
        std::string getCopy(dstring key);
        virtual ~Analyser();
        Analyser(mapping_tp & mtp);

        const static string empty_ret;
    };

    class DLL_EXPORT GDoc{
    public:

        Analyser * analyser;
        mapping_tp mapping;

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
