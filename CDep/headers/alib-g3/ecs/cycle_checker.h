#ifndef AECS_CYCLE_CHECKER
#define AECS_CYCLE_CHECKER
#include <type_traits>

namespace alib::g3::ecs{
    namespace detail{
        template<bool Already,class Compare,class... Ts> struct CycleChecker;

        template<bool Already,class Compare,class T,class... Ts> 
            struct CycleChecker<Already,Compare,T,Ts...>{
            constexpr static bool matched = std::is_same_v<Compare,T>;
            constexpr static bool next_already = Already || matched;
            constexpr static bool conflict = (Already&&matched) || CycleChecker<next_already,Ts...>::conflict;
        };

        template<bool Already,class Compare,class T> 
            struct CycleChecker<Already,Compare,T>{ 
            constexpr static bool matched = std::is_same_v<Compare,T>;
            constexpr static bool next_already = Already || matched;
            constexpr static bool conflict = (Already&&matched);
        };

        template<bool Already,class Compare> 
            struct CycleChecker<Already,Compare>{
            constexpr static bool conflict = false;
        };
    };

    template<class... Ts> struct ComponentStack{
        template<class T> using add_t = ComponentStack<T,Ts...>;

        template<class T> constexpr inline static bool check_cycle(){
            return detail::CycleChecker<false,T,Ts...>::conflict;
        }
    };
}


#endif