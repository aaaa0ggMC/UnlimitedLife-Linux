#ifndef ALIB_EM_H_INCLUDED
#define ALIB_EM_H_INCLUDED
#include <alib-g3/ecs/entity.h>
#include <alib-g3/ecs/component_pool.h>
#include <alib-g3/ecs/linear_storage.h>
#include <memory>

namespace alib::g3::ecs{
    template<class T,std::is_pointer... Ts> IsDepTuple = std::is_same_v<T,std::tuple<Ts...>>;

    namespace detail{
        template<class... Ts> class ComponentStack{
            std::tuple<Ts...> tuple;
            
            template<class T> ComponentStack<T,Ts...> add(){
                return ComponentStack<T,Ts...>();
            }
        };
    };

    struct DLL_EXPORT EntityManager{
    private:
        std::unordered_map<uint64_t,std::unique_ptr<void,void(*)(void*)>> component_pool;
        detail::LinearStorage<Entity> entities;
        id_t id_max;

        template<class T> ComponentPool<T>* get_component_pool_unsafe(){
            auto it = component_pool.find(typeid(T).hash_code());
            if(it == component_pool.end())return nullptr;
            return (ComponentPool<T>*)it->second.get();
        }

        template<class T> inline static void component_pool_destroyer(void* ptr){
            delete static_cast<T*>(ptr);
        }

        template<class T> inline ComponentPool<T>* add_component_pool(){
            auto hash_code = typeid(T).hash_code();
            auto it = component_pool.find(hash_code);
            
            if(it != component_pool.end()){
                return (ComponentPool<T>*)it->second.get();
            }
            auto iter = component_pool.emplace(hash_code,
                std::unique_ptr<void,void(*)(void*)>(
                    (void*)(new ComponentPool<T>),
                    &(EntityManager::component_pool_destroyer<ComponentPool<T>>)
                )
            );
            ComponentPool<T> * pool = (ComponentPool<T>*)iter.first->second.get();
            pool->destroyer = [](void * pobj,id_t entity_id)->int{
                ComponentPool<T> & obj = *(ComponentPool<T>*)(pobj);
                auto it = obj.mapper.find(entity_id);
                if(it == obj.mapper.end())return -1;
                if constexpr(requires(T & t){t.cleanup();}){
                    // 你是有非销毁不可的理由吗？
                    obj.data.data[it->second].cleanup();
                }
                obj.data.remove(it->second);
                obj.mapper.erase(it);
                return 0;
            };
            return pool;
        }
    public:

        inline EntityManager(){
            id_max = 0;
        }
 
        inline Entity create_entity(){
            if(entities.free_elements.empty()){
                std::cout << "新的entity" << std::endl;
                // 从这里可以看到entities的id是和LinearStorage的index基本对齐的 index = id - 1
                return entities.next(++id_max);
            }else{
                std::cout << "复用entity" << std::endl;
                return entities.next_free();
            }
        }

        inline void destroy_entity(const Entity & e){
            for(auto & ref_comp : component_pool){
                //@Note: its data is not usable!!!
                auto cp = (ref_comp.second.get());
                auto destroyer = ((ComponentPool<int>*)cp)->destroyer;
                // 孩子们，我可能会崩溃吗，应该不可能吧
                destroyer(cp,e.id);
            }
            entities.remove(e.id - 1);
        }

        template<class T> T* get_component(const Entity & e){
            ComponentPool<T> * p = get_component_pool_unsafe<T>();
            if(!p)return nullptr;
            auto it = p->mapper.find(e.id);
            return (it == p->mapper.end())?nullptr:&(p->data.data[it->second]);
        }

        /// 语义上保证非空
        template<class T,class Tuo = std::tuple<void>,class... Args> T* add_component(const Entity & e,Args&&... args){
            ComponentPool<T> * p = add_component_pool<T>();
            auto it = p->mapper.find(e.id);
            if(it != p->mapper.end())return &(p->data.data[it->second]);
            // 创建新的component
            // 依赖
            if(requires{T::Dependency;}){
                static_assert<
                [](EntityManager & em,const Entity & e,auto&&... t){
                    ((em.add_component<decltype(t)>(e)),...);
                }(*this,e,T::Dependency());
            }

            // 具体创建
            bool flag;
            size_t index;
            T & comp = p->data.try_next_with_index(flag,index,std::forward<Args>(args)...);
            p->mapper.emplace(e.id,index);
            if(flag)std::cout << "新的component" << std::endl;
            else std::cout << "复用component" << std::endl;
            return &comp;
        }

        enum DestroyResult{
            DRSuccess  = 0,
            DRNoPool   = 1,
            DRCantFind = 2
        };

        template<class T> DestroyResult destroy_component(const Entity & e){
            ComponentPool<T> * p = get_component_pool_unsafe<T>();
            if(!p)return DRNoPool;
            if(p->destroyer((void*)p,e.id) == -1){
                std::cout << "都没找到" << std::endl;
                return DRCantFind;
            }
            std::cout << "成功删除" << std::endl;
            return DRSuccess;
        }

        template<class T,class F> inline void update(F && f){
            ComponentPool<T> * pool = get_component_pool_unsafe<T>();
            if(!pool)return;
            pool->data.available_bits.for_each_skip_1_bits([func = std::forward<F>(f),pool](size_t index){
                func(pool->data.data[index]);
            },pool->data.data.size());
        }
    };

    struct DLL_EXPORT EntityWrapper{
        EntityManager & em;
        Entity e;

        EntityWrapper(EntityManager & manager):em(manager){

        }
    };
}

#endif