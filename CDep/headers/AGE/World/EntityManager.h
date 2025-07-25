/**
 * @file EntityManager.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 实体管理器
 * @version 0.1
 * @date 2025/07/25
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/06/21 
 */
#ifndef AGE_H_EM
#define AGE_H_EM
#include <AGE/World/Entity.h>
#include <AGE/Utils.h>

#include <cstdint>
#include <cstdlib>
#include <unistd.h>
#include <unordered_map>
#include <memory>
#include <vector>
#include <optional>

#ifdef AGE_EM_DEBUG
#include <iostream>
#endif

namespace age::world{
    /* 使用concept我无法写出可以接受随便的参数的concept,所以这里抛弃，改为用户约定
     * template<class T> concept CanUpdate = requires(T&t){
        t.run;
    } || requires (T&t){
        t.update;
    };*/

    template<class T> struct AGE_API ComponentPool{
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

        /// indicator
        bool hasNoFreeEnts;

        inline EntityManager(){
            id_max = AGE_NULL_OBJ;
            hasNoFreeEnts = true;
        }

        //perf[1]: 5.6ns/call
        inline Entity createEntity(){
            // 2025/6/22 aaaa0ggmc
            // 优化前 1e8 用时 3-4s     Entity本位构造(单纯执行Entity(1)): 260ms
            // 问题出在free_entities.empty 耗时太久！
            // origin: if(free_entities.empty()){
            // 优化后 1e8 用时 560ms
            if(hasNoFreeEnts){
                return Entity(++id_max);
            }else{
                auto it = free_entities.begin();
                uint64_t id = *it;
                free_entities.erase(it);
                if(free_entities.empty())hasNoFreeEnts = true;
                return {id};
            }
        }

        //perf[1]: 80ns/call (with no Components)
        inline void destroyEntity(Entity e){
            // 2025/6/22 aaaa0ggmc
            // 优化前 1e8 9175.65ms
            //    个体:
            //        hasNoFreeEnts = false; ==> 350ms
            //        if(hasNoFreeEnts)hasNoFreeEnts = false; ==> 300ms
            //search for its presence,it's much more complex...
            //优化后 1e8 8000ms

            //if(compPool.empty())有必要吗？？完全没有必要！！因为实战情况下不可能为empty
            for(auto & ref_comp : compPool){
                auto cp = (ref_comp.second.get());//@Note: it's data is not usable!!!
                auto destroyer = ((ComponentPool<int>*)cp)->destroyer;
                if(destroyer){
                    destroyer(cp,e.id);
                }
            }
            if(hasNoFreeEnts)hasNoFreeEnts = false;
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
                size_t index = 0;
#ifndef AGE_NO_COMP_REQUISITIONS
                ////Deal with requisitions
                if constexpr (requires { T::requisitions; }){
#ifdef AGE_EM_DEBUG
                    std::cout << "Req count:" << T::requisitions.getContSize() << std::endl;
#endif
                    ///If the required component has no constructor that needs no args,compilation would fail
                    T::requisitions.action([]<class Need>(EntityManager &em,const Entity & e){
                        em.addComponent<Need>(e);
                    },*this,e);
                }
#endif
                if(comp->free_comps.empty()){
                    //create new
                    comp->data.emplace_back(args...);
                    //store mapper
                    index = comp->data.size()-1;
#ifdef AGE_EM_DEBUG
                    std::cout << "append new:" << typeid(T).name()  << " mapper:" << e.id << "->"  << comp->data.size()-1 << std::endl;
#endif
                }else {
                    auto dt = comp->free_comps.begin();
                    index= *dt;
                    comp->free_comps.erase(dt);
#ifdef AGE_EM_DEBUG
                    std::cout << "reuse: " << typeid(T).name() << std::endl;
#endif
                }
                comp->mapper.emplace(e.id,comp->data.size()-1);
                if constexpr(sizeof...(Ts) == 0)comp->data[comp->data.size()-1].reset();
                else comp->data[comp->data.size()-1] = T(args...);
                return &(comp->data[index]);

            }else return &(comp->data[cmp->second]);
        }

        template<class T,class... Ts> inline void update(Ts&&... args)
    requires
        requires(T&t,Ts&&... ts){t.run(ts...);} ||
        requires(T&t,Ts&&... ts){t.update(ts...);}{
            auto opt_pool = getComponentPool<T>();
            if(!opt_pool)return;
            auto pool = (*opt_pool);
            for(auto & [_,index] : pool->mapper){
                auto& runner = pool->data[index];
                if constexpr(requires(T & t,Ts&&... ts){ t.run(ts...); }){
                    runner.run(args...);
                }else runner.update(args...);
            }
        }

    };

    ///@warning EntityWrapper is not that safe.It gives you convenience,but you need to guarantee your behaviour is nice
    struct AGE_API EntityWrapper{
    public:
        Entity e;
        EntityManager & em;

        //perf[1]:
        //perf[1](pre): 518.876 ns/call
        template<class T> inline std::optional<T*> get(){
            return em.getComponent<T>(e);
        }

        template<class T> inline T* get_unsafe(){
            return *em.getComponent<T>(e);
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
        // record 2025/07/18 奇了怪了，为什么正常的东西unwarp后也会segmentfault???我直接解引用就没事
    }


    /// use it with caution
    template<class T> struct ComponentWrapper{
    public:
        std::vector<T> * pool_data;
        size_t index;

        inline ComponentWrapper(){
            pool_data = nullptr;
            index = 0;
        }

        inline void build(EntityManager&em,uint64_t entity_id){
            auto pool = em.getComponentPool<T>();
            if(!!pool){
                pool_data = &((*pool)->data);
                index = (*pool)->mapper[entity_id];
            }else{
                pool_data = NULL;
                index = 0;
            }
        }

        //perf[1]: 17ns/call
        inline T& operator *(){
            return (*pool_data)[index];
        }

        inline T* operator ->(){
            return &((*pool_data)[index]);
        }
    };
}

#endif
