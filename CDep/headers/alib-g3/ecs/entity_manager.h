/**
 * @file entity_manager.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 实体管理
 * @version 0.1
 * @date 2025/11/29
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/11/27 
 */
#ifndef ALIB_EM_H_INCLUDED
#define ALIB_EM_H_INCLUDED
#include <alib-g3/ecs/entity.h>
#include <alib-g3/ecs/component_pool.h>
#include <alib-g3/ecs/linear_storage.h>
#include <alib-g3/ecs/cycle_checker.h>
#include <alib-g3/ecs/component_concepts.h>
#include <alib-g3/aref.h>
#include <alib-g3/adebug.h>
#include <memory>

namespace alib::g3::ecs{
    /**
     * @brief 实体管理器
     * @start-date 2025/11/27
     */
    struct DLL_EXPORT EntityManager{
    private:
        friend class EntityWrapper;
        /// @brief 组件池
        std::unordered_map<uint64_t,std::unique_ptr<void,void(*)(void*)>> component_pool;
        /// @brief 存储实体的线性存储池
        detail::LinearStorage<Entity> entities;
        /// @brief 最大的id,id从1开始
        id_t id_max;
        /// @brief 创建新的组件池子的时候预留的组件数量
        size_t pool_reserve_size;

        /// @brief 获取对应类型的对应组件池
        /// @return 组件池，找不到返回nullptr
        template<class T> ComponentPool<T>* get_component_pool_unsafe(){
            auto it = component_pool.find(typeid(T).hash_code());
            if(it == component_pool.end())return nullptr;
            return (ComponentPool<T>*)it->second.get();
        }

        /// @brief unique_ptr需要一个deleter
        template<class T> inline static void component_pool_destroyer(void* ptr){
            delete static_cast<T*>(ptr);
        }

        /// @brief 获取对应component
        /// @param e 对应的entity
        /// @param[out] index component的索引
        /// @param[out] p 对应组件池的类型，找不到组件池返回nullptr，找不到component则设置索引为size_t::max 
        template<class T> void get_component_impl(const Entity & e,size_t& index,ComponentPool<T>* &p){
            p = get_component_pool_unsafe<T>();
            if(!p)return;
            auto it = p->mapper.find(e.id);
            if(it == p->mapper.end()){
                index = std::numeric_limits<size_t>::max();
            }else{
                index = it->second;
            }
        }
    public:
        /// @brief 获取对应的安全引用
        template<class T> using ref_t = RefWrapper<detail::LinearStorage<T>>;

        /// @brief 获取当前实体池子的大小
        inline size_t get_entity_pool_size(){
            return entities.size();
        } 

        /// @brief 添加一个新的组件池
        /// @return 组件池的指针，保证不为nullptr
        template<class T> inline ComponentPool<T>* add_component_pool(){
            auto hash_code = typeid(T).hash_code();
            auto it = component_pool.find(hash_code);
            
            if(it != component_pool.end()){
                return (ComponentPool<T>*)it->second.get();
            }
            auto iter = component_pool.emplace(hash_code,
                std::unique_ptr<void,void(*)(void*)>(
                    (void*)(new ComponentPool<T>(pool_reserve_size)),
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
                if constexpr(NeedBind<T>){
                    obj.data.data[it->second].bind(Entity::null());
                }
                obj.data.remove(it->second);
                obj.mapper.erase(it);
                return 0;
            };
            return pool;
        }

        /// @brief 确保并获取对应类型的组件池
        /// @tparam T 组件类型
        /// @return 组件池的指针，保证不为nullptr
        template<class T> inline ComponentPool<T>& get_or_create_pool(){
            // 实际调用原来的 add_component_pool 或将实现移动到这里
            return *add_component_pool<T>(); 
        }

        /// @brief 获取对应类型的对应组件池
        /// @return 组件池，找不到返回nullptr
        template<class T> inline ComponentPool<T>* get_component_pool(){
            return get_component_pool_unsafe<T>();
        }

        /// @brief 获取持有的entity线性存储
        inline auto& get_entities_storage(){
            return entities; 
        }

        /// @brief 构造实体表
        /// @param entity_reserve_size 实体预留数量 
        /// @param pool_res 池子预留数量
        inline EntityManager(size_t entity_reserve_size = 0,size_t pool_res = 0)
        :entities(entity_reserve_size){
            pool_reserve_size = pool_res;
            id_max = 0;
        }
 
        /// @brief 创建一个新的实体
        /// @return 实体
        inline Entity create_entity(){
            if(entities.free_elements.empty()){
                // 从这里可以看到entities的id是和LinearStorage的index基本对齐的 index = id - 1
                return entities.next(++id_max);
            }else{
                return entities.next_free();
            }
        }

        /// @brief 销毁实体
        /// @param e 实体，只看id
        inline void destroy_entity(const Entity & e){
            for(auto & ref_comp : component_pool){
                //@Note: its data is not usable!!!
                auto cp = (ref_comp.second.get());
                auto destroyer = ((detail::PoolDestroyerBase*)cp)->destroyer;
                // 孩子们，我可能会崩溃吗，应该不可能吧
                destroyer(cp,e.id);
            }
            entities.remove(e.id - 1);
        }

        /// @brief 获取实体原始的组件数据
        /// @param e 实体
        /// @return 指针，nullptr表示没找到
        template<class T> T* get_component_raw(const Entity & e){
            size_t index;
            ComponentPool<T> * p;
            get_component_impl<T>(e,index,p);
            if(p && index != std::numeric_limits<size_t>::max())return &((*p).data.data[index]);
            else return nullptr;
        }

        /// @brief 获取对应实体对应组件的安全引用
        /// @param e 实体
        /// @return nullopt表示没有对应组件，否则返回组件的安全引用
        template<class T> std::optional<ref_t<T>> get_component(const Entity & e){
            size_t index;
            ComponentPool<T> * p;
            get_component_impl<T>(e,index,p);
            if(p && index >= 0)return ref((p->data),index);
            else return std::nullopt;
        }
        
        /// @brief 添加一个新的组件，支持依赖也进行添加，会识别循环依赖
        /// @tparam T 添加的类型
        /// @tparam Tuo 目前的组件链条
        /// @param e 对应的实体
        /// @param ...args 组件T的构造函数，遗憾的是依赖的类型只能使用空参数的构造函数
        /// @return 对应的安全引用，语义上保证非空
        template<class T,class Tuo = ComponentStack<> ,class... Args> EntityManager::ref_t<T> add_component(const Entity & e,Args&&... args){
            ComponentPool<T> * p = add_component_pool<T>();
            auto it = p->mapper.find(e.id);
            if(it != p->mapper.end())return ref(p->data,it->second);
            // 创建新的component
            // 依赖
            if constexpr(requires{typename T::Dependency;}){
                using Tuo_Next = typename Tuo::add_t<T>;
                // 检测循环依赖
                static_assert(!Tuo_Next::template check_cycle<T>(),"Cycle dependency!");

                []<class... TArgs>(EntityManager & em,const Entity & e,ComponentStack<TArgs...>){
                    ((em.add_component<TArgs,Tuo_Next>(e)),...);
                }(*this,e,typename T::Dependency{});
            }

            // 具体创建
            bool flag;
            size_t index;
            T & comp = p->data.try_next_with_index(flag,index,std::forward<Args>(args)...);
            if constexpr(NeedBind<T>){
                comp.bind(e);
            }
            p->mapper.emplace(e.id,index);
            return ref(p->data,index);
        }

        /// @brief 移除组件的返回值
        enum DestroyResult{
            DRSuccess  = 0, ///< 成功删除组件
            DRNoPool   = 1, ///< 没有对应的组件池
            DRCantFind = 2  ///< 组件池找不到对应entity的mapper 
        };

        /// @brief 移除对应的component
        /// @tparam T 组件的类型
        /// @param e 实体
        /// @return 移除组件的结果
        template<class T> DestroyResult remove_component(const Entity & e){
            ComponentPool<T> * p = get_component_pool_unsafe<T>();
            if(!p)return DRNoPool;
            if(p->destroyer((void*)p,e.id) == -1){
                return DRCantFind;
            }
            return DRSuccess;
        }

        /// @brief 更新所有的非空闲的组件
        /// @tparam T 组件池类型
        /// @param f 函数类型
        template<class T,class F> inline void update(F && f){
            ComponentPool<T> * pool = get_component_pool_unsafe<T>();
            if(!pool)return;
            pool->data.available_bits.for_each_skip_1_bits([func = std::forward<F>(f),pool](size_t index){
                func(pool->data.data[index]);
            },pool->data.data.size());
        }

        /// @brief 更新所有的非空闲的组件
        /// @tparam T 组件池类型，组件内需要有update函数
        /// @param Args
        template<class T,class... Args> inline void update(Args&&... args){
            static_assert(requires(T & t,Args&&... t_args){t.update(t_args...);},"There is not update function in the component.");
            ComponentPool<T> * pool = get_component_pool_unsafe<T>();
            if(!pool)return;
            
            auto args_tuple = std::make_tuple(std::forward<Args>(args)...);

            pool->data.available_bits.for_each_skip_1_bits(
                [pool,args_tuple = std::move(args_tuple)](size_t index) mutable{
                    std::apply(
                        [&](auto&&... func_args){
                            pool->data.data[index].update(std::forward<decltype(func_args)>(func_args)...);
                        },
                        args_tuple
                    );
                },pool->data.data.size()
            );
        }
    };

    /// @brief 实体的wrapper
    struct DLL_EXPORT EntityWrapper{
    private:
        /// @brief 实体管理
        EntityManager & em;
        /// @brief 实体
        Entity e;
    public:
        /// @brief 获取实体
        Entity get_entity(){
            return e;
        }

        /// @brief 实体的wrapper
        /// @param manager 对应的manager
        EntityWrapper(EntityManager & manager):em(manager){
            e = manager.create_entity();
        }

        /// @brief 从已经存在的实体进行包装吗，其实会更新版本号码
        /// @param et 实体引用
        EntityWrapper(EntityManager & manager,const Entity & et):em(manager){
            set_entity(et);
        }

        /// @brief 检查当前的实体是否为空
        bool is_null(){
            return e.id == 0;
        }

        /// @brief 获取对应的组件
        /// @return nullopt表示找不到组件
        template<class T> std::optional<EntityManager::ref_t<T>> get(){
            panic_debug(e.id == 0,"Invalid handle.");
            return em.get_component<T>(e);
        }

        /// @brief 添加一个新的组件
        /// @return 组件的安全引用
        template<class T,class... Args> EntityManager::ref_t<T> add(Args&&... args){
            panic_debug(e.id == 0,"Invalid handle.");
            return em.add_component<T>(e,std::forward<Args>(args)...);
        }

        /// @brief 添加若干组件
        /// @tparam ...Cs 组件类型
        /// @return 组件安全引用的tuple，无法通过std::get<基础类型获取>，可以通过alib::g3::ecs::get<基础类型>获取
        template<class... Cs> std::tuple<EntityManager::ref_t<Cs>...> adds(){
            panic_debug(e.id == 0,"Invalid handle.");
            return std::make_tuple(
                em.add_component<Cs>(e)...
            );
        }

        /// @brief 移除组件，由于复杂度原因不支持反向依赖删除组件
        /// @tparam T 类型
        template<class T> void remove(){
            panic_debug(e.id == 0,"Invalid handle.");
            if(e.id)em.remove_component<T>(e);
        }

        /// @brief 设置entity，会检测entity是否有效（null&valid&version check)
        /// @param et 
        void set_entity(const Entity & et){
            if(!et.id || et.id > em.entities.size()){
                e = Entity::null();
                return;
            }
            if(!em.entities.available_bits.get(et.id - 1)){
                e = Entity::null();
            }else{
                Entity & latest = em.entities[et.id - 1];
                if(latest.version != et.version){
                    e = Entity::null();
                }else e = latest;
            }
        }

        /// @brief 删除实体
        void destroy(){
            panic_debug(e.id == 0,"Invalid handle.");
            if(e.id == 0)return;

            em.destroy_entity(e);
            e = Entity::null();
        }
    };

    /// @brief 获取adds返回的tuple对应位置的引用
    /// @tparam T 类型
    /// @tparam ...Ts 返回的tuple中的类型表，为RefWrapper<X>...
    /// @param t 对应的右值tuple
    /// @return 对应的安全引用
    template<class T,class... Ts> inline EntityManager::ref_t<T>
        get(std::tuple<EntityManager::ref_t<Ts>...> && t){
            return std::move(std::get<EntityManager::ref_t<T>>(t));
    }

    /// @brief 获取adds返回的tuple对应位置的引用
    /// @tparam T 类型
    /// @tparam ...Ts 返回的tuple中的类型表，为RefWrapper<X>...
    /// @param t 对应的左值tuple
    /// @return 对应的安全引用
    template<class T,class... Ts> inline EntityManager::ref_t<T>&
        get(std::tuple<EntityManager::ref_t<Ts>...> & t){
            return std::get<EntityManager::ref_t<T>>(t);
    }

    /// @brief 简易包装
    template<class T> using ref_t = EntityManager::ref_t<T>;
}

#endif