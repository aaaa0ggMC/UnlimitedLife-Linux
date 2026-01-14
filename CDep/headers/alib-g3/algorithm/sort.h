/**
 * @file sort.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 各种排序算法，在reverse=false的情况下，若使用defcompare，即arg1 < arg2,保证升序
 * @version 0.1
 * @date 2026/01/14
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/12/17 
*/
#ifndef ALIB_ALGO_SORT_H_INCLUDED
#define ALIB_ALGO_SORT_H_INCLUDED
#include <utility>
#include <iterator>
#include <vector>

namespace alib::g3::algo::sort{

    template<class T> using default_compare = std::less<T>;
    template<class Fn> auto wrap_compare(Fn && fn){
        return [f = std::forward<decltype(fn)>(fn)]<class Tp>(const Tp & a,const Tp & b,bool reverse){
            return reverse ? f(b,a) : f(a,b);
        };
    }

    template<class Fn> auto wrap_inject(Fn && fn){
        return [f = std::forward<decltype(fn)>(fn)](std::size_t prev,std::size_t aft){
            if constexpr(!std::is_same_v<std::decay_t<decltype(f)>,std::nullptr_t>){
                f(prev,aft);
            }
        };
    }
    
    template<class Fn,class T> concept IsCompareFn = 
        requires(Fn && fn,const T& a,const T& b,bool & m){
            { fn(a,b) } -> std::convertible_to<bool>;
        };
    template<class Fn> concept IsInjectFn = 
        requires(Fn && fn,std::size_t swap_prev,std::size_t swap_aft){
            fn(swap_prev,swap_aft);
        } || 
        requires(){
            std::is_same_v<std::decay_t<Fn>,std::nullptr_t>;
        };
    template<class T> concept IsIterator = 
        std::random_access_iterator<T> ||
        std::is_pointer_v<T>;

    /// @brief 基础的插入排序，时间复杂度O(n^2)   Tier: D
    /// @param begin   开始
    /// @param end     结束
    /// @param compare 比较函数 
    /// @param reverse 是否反转排序方向
    /// @param inject  每次进行一次数据交换后进行的操作
    template<IsIterator IterType,
            IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
            IsInjectFn InjectFn = std::nullptr_t
            > 
    void insertion(
        IterType begin,
        IterType end,
        CompareFn&& i_compare,
        bool reverse = false,
        InjectFn && i_inject = nullptr
    ){
        using value_t = std::iterator_traits<IterType>::value_type;
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));
        if(begin == end)return;

        for(IterType current = (begin + 1);current < end;++current){
            value_t val = std::move(*current);
            IterType j = current;

            while(j > begin && compare(val,*(j-1),reverse)){
                *j = std::move(*(j-1));
                inject(std::distance(begin,j),std::distance(begin,j-1));
                --j;
            }
            *j = std::move(val);
        }
    }
    
    /// @brief 基础的冒泡排序，时间复杂度O(n^2) Tier:E
    /// @param begin   开始
    /// @param end     结束
    /// @param compare 比较函数 
    /// @param reverse 是否反转排序方向
    /// @param inject  每次进行一次数据交换后进行的操作
    template<IsIterator IterType,
            IsCompareFn<typename std::iterator_traits<IterType>::value_type>  CompareFn,
            IsInjectFn InjectFn = std::nullptr_t
        > 
    void bubble(
        IterType begin,
        IterType end,
        CompareFn&& i_compare,
        bool reverse = false,
        InjectFn && i_inject = nullptr
    ){
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));
        if(begin == end)return;
    
        for(IterType current = begin;current < (end - 1);++current){
            for(IterType i_current = begin;i_current < (end - 1 - (current - begin));++i_current){
                if(compare(*(i_current+1),*i_current,reverse)){
                    std::swap(*i_current,*(i_current+1));
                    inject(std::distance(begin,i_current),std::distance(begin,i_current + 1));
                }
            }
        }
    }

    /// @brief 快排Lomuto分区方式
    struct PartitionLomuto {
        template<IsIterator IterType,class CompareFn,class InjectFn,class Node> 
            static void partition(IterType low,IterType high,
                CompareFn& compare,bool reverse,InjectFn & inject,
                std::vector<Node> & nodes,IterType begin,IterType end
            ){
                IterType i = low;

                for(IterType cur = low;cur < high;++cur){
                    if(compare(*cur,*high,reverse)){
                        if(i != cur){
                            std::swap(*i,*cur);
                            inject(std::distance(begin,i),std::distance(begin,cur));
                        }
                        ++i;
                    }
                }
                std::swap(*i,*high);
                inject(std::distance(begin,i),std::distance(begin,high));
                
                if(i > (low+1)){
                    nodes.emplace_back(low,i-1);
                }
                if(i < (high - 1)){
                    nodes.emplace_back(i+1,high);
                }
            }
    };

    /// @brief 快排Hoare分区方式
    struct PartitionHoare {
        template<IsIterator IterType, class CompareFn, class InjectFn, class Node>
        static void partition(IterType low, IterType high,
                            CompareFn& compare, bool reverse, InjectFn& inject,
                            std::vector<Node>& nodes,IterType begin,IterType end
        ){
            using value_t = typename std::iterator_traits<IterType>::value_type;
            IterType pivot = (low + (high - low) / 2);

            IterType l = low;
            IterType h = high;

            while(l <= h){
                while(compare(*l,*pivot,reverse)) ++l;
                while(compare(*pivot,*h,reverse)) --h;
                
                if(l <= h){
                    if(l != h){
                        [[unlikely]] if(l == pivot){
                            pivot = h;
                        }else [[unlikely]] if(h == pivot){
                            pivot = l;
                        }

                        std::swap(*l, *h);
                        inject(std::distance(begin,l),std::distance(begin,h));
                    }
                    ++l;
                    --h;
                }
            }

            if(low < h){
                nodes.emplace_back(low, h);
            }
            if(l < high){
                nodes.emplace_back(l, high);
            }
        }
    };

    /// @brief 三数取中算法
    struct PartitionMedianThree {
        template<IsIterator IterType, class CompareFn, class InjectFn, class Node>
        static void partition(IterType low, IterType high,
                            CompareFn& compare, bool reverse, InjectFn& inject,
                            std::vector<Node>& nodes,IterType begin,IterType end
        ){
            IterType pivot = (low + (high - low) / 2);

            // pivot > low ?
            bool c1 = compare(*low,*pivot,false);
            // pivot < high ?
            bool c2 = compare(*pivot,*high,false);
        
            if(!(c1 && c2)){
                // low < high
                bool c3 = compare(*low,*high,false);
                if(!c1 && c2){
                    std::swap(*low,*pivot);
                    inject(std::distance(begin,low),std::distance(begin,pivot));
                }else if(c1 && !c2){
                    std::swap(*high,*pivot);
                    inject(std::distance(begin,high),std::distance(begin,pivot));
                }
            }

            PartitionHoare::partition(low,high,compare,
                reverse,inject,nodes,begin,end);
        }
    };

    /// @brief 基础的快速排序，时间复杂度O(nlogn) Tier:B
    /// @param begin   开始
    /// @param end     结束
    /// @param compare 比较函数 
    /// @param reverse 是否反转排序方向
    /// @param inject  每次进行一次数据交换后进行的操作
    template<class PartitionMethod = PartitionMedianThree,IsIterator IterType,
            IsCompareFn<typename std::iterator_traits<IterType>::value_type> CompareFn,
            IsInjectFn InjectFn = std::nullptr_t
    > 
    void quick(
        IterType begin,
        IterType end,
        CompareFn&& i_compare,
        bool reverse = false,
        InjectFn && i_inject = nullptr
    ){
        using value_t = std::iterator_traits<IterType>::value_type;
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));
        if(begin == end)return;
    
        // 这里手动实现一个简单的stack
        struct Node{
            IterType low;
            IterType high;

            Node(IterType l,IterType h):low{l},high{h}{}
        };

        std::vector<Node> stack;
        stack.reserve(256);

        stack.emplace_back(begin,end-1);
        while(!stack.empty()){
            // partition
            Node poped = stack.back();
            stack.pop_back();

            IterType low = poped.low;
            IterType high = poped.high;

            PartitionMethod::partition(low,high,compare,
                        reverse,inject,stack,begin,end);
        }
    }
}

 #endif