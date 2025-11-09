/**
 * @file aref.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 不会悬垂的比较安全的容器数据wrapper,Release下单次性能损失为0.3ns
 * @version 0.1
 * @date 2025/11/09
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/11/08 
 */
#ifndef AREF_H_INCLUDED
#define AREF_H_INCLUDED
#include <concepts>
#include <type_traits>
#include <cassert>

namespace alib::g3{
    template<class T> concept CanAccessItem = requires(T&t){
        { t[0] } -> std::same_as<typename T::reference>;
    };

    /// @brief 通过持有index从而保证一定的安全性，Release优化后仅仅多出0.3ns(5.7Ghz电脑)的解引用时长
    /// @tparam Cont 
    template<CanAccessItem Cont> struct RefWrapper{
        using BaseType = Cont::value_type;
        /// 容器的引用，所以你需要保证至少容器是存在的，嵌套容器就可能不太支持了
        Cont & cont;
        /// index
        size_t index;
    
        inline auto& get(){
            // debug下防止shrink
            assert(index < cont.size());
            return cont[index];
        }

        inline auto* operator->(){
            return &(get());
        }

        template<class U> inline auto operator=(U&& val) 
            -> decltype(cont[index] = std::forward<U>(val))
        {
            return get() = std::forward<U>(val);
        }

        inline RefWrapper<Cont> operator+(int offset){
            auto t = *this;
            t.set_index(index + offset);
            return t;
        }

        inline RefWrapper<Cont> operator-(int offset){
            auto t = *this;
            t.set_index(index - offset);
            return t;
        }

        inline RefWrapper<Cont>& operator+=(int offset){
            return set_index(index + offset);
        }

        inline RefWrapper<Cont>& operator-=(int offset){
            return set_index(index - offset);
        }

        inline RefWrapper<Cont>& operator++(){
            return set_index(index + 1);
        }

        inline RefWrapper<Cont>& operator--(){
            return set_index(index - 1);
        }

        inline RefWrapper<Cont> operator++(int){
            auto ret = *this;
            set_index(index + 1);
            return ret;
        }
        
        inline RefWrapper<Cont> operator--(int){
            auto ret = *this;
            set_index(index - 1);
            return ret;
        }

        inline RefWrapper<Cont>& set_index(size_t index){
            assert(index < cont.size());
            this->index = index;
            return *this;
        }
    };

    template<CanAccessItem Cont> auto 
        ref(Cont & cont,size_t index){
        assert(index < cont.empty());
        return RefWrapper<Cont>{cont,index};
    }
}

#endif