#include <unordered_map>
#include <optional>
#ifdef FOONATHAN_MEMORY_MEMORY_POOL_HPP_INCLUDED
    #define MemPool memory::memory_pool
#else
    #define MemPool void
#endif

namespace age{
    class ResourceManager{
    public:
        MemPool * mempool;

        std::unordered_map<std::string,std::string *> stringbufs;

        std::optional<std::string *> allocateString(const std::string& str);

        ResourceManager();
        ~ResourceManager();
    };
}
