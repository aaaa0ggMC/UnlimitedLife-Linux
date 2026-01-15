/**
 * @file component_concepts.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 这里列出了entity_manager支持的所有注入方式，主要为文档说明
 * @version 0.1
 * @date 2026/01/15
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/11/29 
 */
#ifndef AECS_COMPONENTS_CONCEPTS
#define AECS_COMPONENTS_CONCEPTS
#include <alib-g3/ecs/entity.h>

//// Component ////
namespace alib::g3::ecs{
    /// 如果你的component中有需要在entity.remove_component<T>()时立马销毁的数据，可以通过写cleanup函数来实现
    /// 参数要求:    无参数
    /// 返回值要求：  无要求（因为不会被使用，因此建议为void）
    template<class T> concept NeedCleanup = requires(T& t){t.cleanup();};
    /// @brief 可以通过继承这个基类来方便地表示你需要cleanup
    /// @note  由于并不需要转换成基类来访问，因此其实没有vtable开销
    struct INeedCleanup{
        virtual void cleanup() = 0;
    };

    /// 添加component的时候要是不存在空的槽位会执行构造函数，参数传递
    /// 比如e.add<T>(...) -> emplace<T>(...)，而要是有空的槽位会进行一个判断:
    /// 如果存在reset函数，优先调用reset，否则执行析构再原位使用构造函数构造
    /// 因此建议reset函数的签名和构造函数一致
    /// 参数要求:   任意，但是要和传入的符合
    /// 返回值要求： 无要求，不会被使用
    template<class T,class... Args> concept NeedReset = 
        requires(T&t,Args&&... args){t.reset(std::forward<Args>(args)...);};
    /// @brief 可以通过继承这个类来表示你需要reset，但是这个的继承只有空参列表的，用处不大  
    struct INeedReset{
        virtual void reset() = 0;
    };

    /// 如果你希望在System中能够访问到组件目前绑定的entity，可以使用下面的注入方式
    /// 当存在bind接口的时候，entity_manager会保证在创建组件的时候bind到对应的entity
    /// 然后在删除组件的时候bind到空的entity(Entity::null())
    /// 绑定具体值穿入的是左值，绑定空值传入的是右值
    /// 如果cleanup和bind同时存在，先cleanup在bind空值
    /// 参数要求: 第一个为entity
    /// 返回值要求: 无要求，不会被使用
    template<class T> concept NeedBind = requires(T&t,Entity e){t.bind(e);};
    /// @brief 通过下面进行简单注入
    struct IBindEntity{
        Entity bound_entity;

        inline void bind(const Entity & e){
            bound_entity = e;
        }

        inline const Entity& get_bound(){
            return bound_entity;
        }
    };

    /// 如果你希望获取组件目前的slot可以设置这个，由于组件slot位置都不会变动，因此每次创建component的时候会bind
    /// 复用的时候不会处理
    template<class T> concept NeedSlotId = requires(T& t,size_t index){t.slot(index);};
    /// @brief 简单注入
    struct ISlotComponent{
        size_t m_slot;

        inline void slot(size_t i){
            m_slot = i;
        }

        inline size_t get_slot(){
            return m_slot;
        }
    };

    /// 如果你希望获取你需要注入的依赖，可以使用bind_dep方法来设置，传入的是tuple
    /// 需要Dependency::deptup_t 来获取，所以你的Dependency的类型要为
    /// ComponentStack
    template<class T,class Tup> concept NeedBindDependency = requires(T & t,Tup & tup){
        t.bind_dep(tup);
    };

    /// 这里可以对组件进行依赖
    /// 要求组件内存在 using Dependency = ComponentStack<...>;
    /// 碰到循环依赖会报错
    /// eg.
    /**
     * struct Comp{
     *  using Dependency = alib::g3::ecs::ComponentStack<Velocity,Mass,RigidBody>;
     * };
     */
    template<class T> concept NeedDependency = requires{typename T::Dependency;};

    /// 如果你希望使用entitymanager.update<T>这个原始人函数来统一update你的组件，可以使用这个
    template<class T,class... Args> concept NeedUpdate = requires(T & t,Args&&... t_args){t.update(t_args...);};

    /// 这个是用来检测你的某个类是否达到标准从而拥有某个trait的
    template<class T> struct ComponentTraits{
        constexpr static bool dependency = NeedDependency<T>;
        
        constexpr static bool __get_bind_dependency(){
            if constexpr(dependency){
                if constexpr(NeedBindDependency<T,typename T::Dependency::deptup_t>){
                    return true;
                }
                return false;
            }
            return false;
        }

        constexpr static bool bind_dependency = __get_bind_dependency();
        constexpr static bool cleanup = NeedCleanup<T>;
        constexpr static bool bind = NeedBind<T>;
        constexpr static bool slot_id = NeedSlotId<T>;
        template<class... Args> constexpr static bool reset = NeedReset<T,Args...>;
        template<class... Args> constexpr static bool update = NeedUpdate<T,Args...>;
    
        template<class Target,class EndToken> static void check(Target && t,EndToken && end_token){
            auto simp_yn = [](bool val){
                return val?"Y":"N";
            };
            
            t << typeid(T).name() << ":\n"
            "\tDependency            :" << simp_yn(dependency) << "\n"
            "\tCleanup               :" << simp_yn(cleanup) << "\n"
            "\tBindEntity            :" << simp_yn(bind) << "\n"
            "\tBindSlotId            :" << simp_yn(slot_id) << "\n"
            "\tBindDependency        :" << simp_yn(bind_dependency) << "\n"
            "\tHasEmptyReset         :" << simp_yn(requires(T && t){t.reset();}) << "\n"
            "\tHasEmptyUpdate        :" << simp_yn(requires(T && t){t.update();})
            << end_token;
        }
    };
}
#endif