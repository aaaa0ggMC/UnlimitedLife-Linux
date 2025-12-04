/**
 * @file cycle_checker.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 对循环依赖的组件进行检测
 * @version 0.1
 * @date 2025/12/04
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/11/27 
 */
#ifndef AECS_CYCLE_CHECKER
#define AECS_CYCLE_CHECKER
#include <alib-g3/aref.h>
#include <alib-g3/ecs/linear_storage.h>
#include <type_traits>

namespace alib::g3::ecs{
    namespace detail{
        /// @brief 基类，查找Ts...中是否出现两个Compare
        /// @tparam Compare 比较的内容 
        /// @tparam ...Ts 目前已经存在的类型列表
        /// @tparam Already 是否找到了第一个类型
        template<bool Already,class Compare,class... Ts> struct CycleChecker;

        /// @brief 基础特化，用于构造递归链条
        template<bool Already,class Compare,class T,class... Ts> 
            struct CycleChecker<Already,Compare,T,Ts...>{
            constexpr static bool matched = std::is_same_v<Compare,T>;
            constexpr static bool next_already = Already || matched;
            constexpr static bool conflict = (Already&&matched) || CycleChecker<next_already,Ts...>::conflict;
        };

        /// @brief 终止递归
        template<bool Already,class Compare,class T> 
            struct CycleChecker<Already,Compare,T>{ 
            constexpr static bool matched = std::is_same_v<Compare,T>;
            constexpr static bool next_already = Already || matched;
            constexpr static bool conflict = (Already&&matched);
        };

        /// @brief 用于处理异常情况
        template<bool Already,class Compare> 
            struct CycleChecker<Already,Compare>{
            constexpr static bool conflict = false;
        };
    };


    /// @brief 获取对应的安全引用
    template<class T> using ref_t = RefWrapper<detail::LinearStorage<T>>;
    /// @brief 用于递归的传入链，也是Dependency的依赖链
    /// @tparam ...Ts 类型列表
    template<class... Ts> struct ComponentStack{
        /// @brief 添加一个新的类型
        template<class T> using add_t = ComponentStack<T,Ts...>;
        /// @brief 获取对应的tuple
        using deptup_t = std::tuple<ref_t<Ts>...>;

        /// @brief 检查是否出现循环依赖
        /// @tparam T 要检测的类型
        template<class T> constexpr inline static bool check_cycle(){
            return detail::CycleChecker<false,T,Ts...>::conflict;
        }
    };
}
#endif