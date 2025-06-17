#ifndef AGE_EM
#define AGE_EM
#include <AGE/World/Entity.h>
#include <AGE/Base.h>
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <vector>
#include <optional>

namespace age::world{
    template<class T> struct ComponentPool{
        std::vector<T> data;
        std::unordered_map<uint64_t,size_t> mapper;
    };

    struct AGE_API EntityManager{
    public:
        using namespace comps;

        // <typeid,vector<...>>
        std::unordered_map<uint64_t,std::unique_ptr<void>> compPool;

        std::vector<Entity> entities;
        std::vector<size_t> free_entities;

        EntityManager();

        template<class T> std::optional<ComponentPool<T>*> getComponentPool(){
            auto it = comptypeid(T)
        }

    };

    struct EntityWrapper {
    public:
        EntityWrapper();
    }
}

#endif
