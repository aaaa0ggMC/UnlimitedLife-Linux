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
        std::vector<size_t> free_comps;
        void (*destroyer)(void*,uint64_t);

        inline ComponentPool():destroyer{nullptr}{}
    };

    struct AGE_API EntityManager{
    public:
        // <typeid,vector<...>>
        std::unordered_map<uint64_t,std::unique_ptr<void,void(*)(void*)>> compPool;

        std::vector<Entity> entities;
        std::vector<size_t> free_entities;

        // 0 initial
        size_t id_max;

        inline EntityManager(){
            id_max = AGE_NULL_OBJ;
        }

        inline Entity createEntity(){
            if(free_entities.empty()){
                return Entity(++id_max);
            }else{
                auto it = free_entities.begin();
                uint64_t id = *it;
                free_entities.erase(it);
                return {id};
            }
        }

        inline void destroyEntity(Entity e){
            //search for its presence,it's much more complex...
            for(auto & ref_comp : compPool){
                auto cp = (ref_comp.second.get());//@Note: it's data is not usable!!!
                auto destroyer = ((ComponentPool<int>*)cp)->destroyer;
                if(destroyer){
                    destroyer(cp,e.id);
                }
            }
            free_entities.push_back(e.id);
        }

        template<class T> inline static void componentPoolDeleter(void * ptr){
            delete static_cast<T*>(ptr);
        }

        template<class T> inline std::optional<ComponentPool<T> *> getComponentPool(){
            auto it = compPool.find(typeid(T).hash_code());
            if(it == compPool.end()){
                return std::nullopt;
            }else return {(ComponentPool<T>*)(it->second.get())};
        }

        template<class T> inline std::optional<T*> getComponent(Entity e){
            auto pool = getComponentPool<T>();
            if(!pool)return std::nullopt;
            auto mapping = (*pool)->mapper.find(e.id);
            if(mapping == (*pool)->mapper.end())return std::nullopt;
            return {&((*pool)->data[mapping->second])};
        }

        template<class T> inline void removeComponent(Entity e){
            auto pool = getComponentPool<T>();
            if(!pool)return;
            auto pv = (*pool);
            auto mapping = pv->mapper.find(e.id);
            if(mapping == pv->mapper.end())return;
            if(pv->destroyer){
                pv->destroyer((void*)(pv),e.id);
#ifdef AGE_EM_DEBUG
                std::cout << "deleted: " << typeid(T).name() << std::endl;
#endif
            }
        }

        template<class T,class... Ts> inline T* addComponent(Entity e,Ts&&... args){
            auto pool = getComponentPool<T>();
            ComponentPool<T>* comp = nullptr;

            if(!pool){
                comp = new ComponentPool<T>;
                comp->destroyer = [](void * pobj,uint64_t entity_id){
                    ComponentPool<T> & obj = *(ComponentPool<T>*)(pobj);
                    auto it = obj.mapper.find(entity_id);
                    if(it == obj.mapper.end())return;
                    //make it empty
                    obj.free_comps.push_back(it->second);
                    obj.mapper.erase(it);
                };
                compPool.emplace(typeid(T).hash_code(),std::unique_ptr<void,void(*)(void*)>(
                    (void*)comp,
                    &(EntityManager::componentPoolDeleter<ComponentPool<T>>)
                ));
#ifdef AGE_EM_DEBUG
                std::cout << "new pool:" << typeid(T).name() << std::endl;
#endif
            }else comp = *pool;
            //OK
            auto cmp = comp->mapper.find(e.id);
            if(cmp == comp->mapper.end()){
                if(comp->free_comps.empty()){
                    //create new
                    comp->data.emplace_back(std::forward<Ts>(args)...);
                    //store mapper
                    comp->mapper.emplace(e.id,comp->data.size()-1);
                    if constexpr(sizeof...(Ts) == 0)comp->data[comp->data.size()-1] = T::null();
                    else comp->data[comp->data.size()-1] = T(std::forward<T>(args)...);
#ifdef AGE_EM_DEBUG
                    std::cout << "append new:" << typeid(T).name() << std::endl;
#endif
                    return &(comp->data.back());
                }else {
                    auto dt = comp->free_comps.begin();
                    size_t index= *dt;
                    comp->free_comps.erase(dt);
                    if constexpr(sizeof...(Ts) == 0)comp->data[index] = T::null();
                    else comp->data[index] = T(std::forward<T>(args)...);
                    comp->mapper.emplace(e.id,index);
#ifdef AGE_EM_DEBUG
                    std::cout << "reuse: " << typeid(T).name() << std::endl;
#endif
                    return &(comp->data[index]);
                }
            }else return &(comp->data[cmp->second]);
        }
    };

    struct EntityWrapper{
    public:
        Entity e;
        EntityManager & em;

        template<class T> inline std::optional<T*> get(){
            return em.getComponent<T>(e);
        }

        template<class T,class... Ts> inline T* add(Ts&&... args){
            return em.addComponent<T>(e,args...);
        }

        //Do not support component that needs args,what's more,you can't get the value
        template<class... Ts> inline EntityWrapper& adds(){
            (em.addComponent<Ts>(e),...);
            return *this;
        }

        template<class... Ts> inline EntityWrapper& removes(){
            (em.removeComponent<Ts>(e),...);
            return *this;
        }

        template<class T> inline EntityWrapper& remove(){
            em.removeComponent<T>(e);
            return *this;
        }

        inline void destroy(){
            em.destroyEntity(e);
            e = Entity(AGE_NULL_OBJ);//0 means invalid
        }

        inline EntityWrapper(Entity ent,EntityManager& emm):em{emm}{
            e = ent;
        }
    };

    inline EntityWrapper wrap(Entity e,EntityManager&em){
        return EntityWrapper(e,em);
    }

    ///may cause exception,use it cautiously only when you know that opt mustn't be std::nullopt
    template<class T> inline T* unwrap(std::optional<T*> opt){
        return (*opt); //segment fault may be arisen here
    }
}

#endif
