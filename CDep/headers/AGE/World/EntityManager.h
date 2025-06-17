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
        std::unordered_map<uint64_t,std::unique_ptr<void,void(*)(void*)>> compPool;

        std::vector<Entity> entities;
        std::vector<size_t> free_entities;

        EntityManager();

        template<class T> static void componentPoolDeleter(void * ptr){
            delete static_cast<T*>(ptr);
        }

        template<class T> std::optional<ComponentPool<T> *> getComponentPool(){
            auto it = compPool.find(typeid(T).hash_code());
            if(it == compPool.end()){
                return std::nullopt;
            }else return {(ComponentPool<T>*)(it->second.get())};
        }

        template<class T> std::optional<T&> getComponent(Entity e){
            auto pool = getComponentPool<T>();
            if(!pool)return std::nullopt;
            auto mapping = (*pool).mapper.find(e.id);
            if(mapping == (*pool).mapper.end())return std::nullopt;
            return {(*pool).entites[mapping->second]};
        }

        template<class T,class... Ts> T& addComponent(Entity e,Ts&&... args){
            auto pool = getComponentPool<T>();
            ComponentPool<T>* comp = nullptr;
            if(!pool){
                comp = new ComponentPool<T>;
                compPool.emplace(typeid(T).hash_code(),std::make_unique<void,void(*)(void*)>(
                    (void*)(comp),
                    &(EntityManager::componentPoolDeleter<ComponentPool<T>>)
                ));
            }else comp = *pool;
            //OK
            auto cmp = comp->mapper.find(e.id);
            if(cmp == comp->mapper.end()){
                //create new
                comp->data.emplace_back(std::forward<Ts>(args)...);
                //store mapper
                comp->mapper.emplace(e.id,comp->data.size()-1);
                return comp->data.back();
            }else return comp->data[cmp->second];
        }
    };

    struct EntityWrapper {
    public:
        EntityWrapper();
    };
}

#endif
