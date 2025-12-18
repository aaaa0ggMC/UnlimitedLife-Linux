/**
 * @file sort.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 各种排序算法，在reverse=false的情况下，若使用defcompare，即arg1 > arg2,保证升序
 * @version 0.1
 * @date 2025/12/18
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

    template<class T> bool DefCompare(const T& a,const T& b){
        return a > b;
    }

    auto wrap_compare(auto && fn){
        return [f = std::forward<decltype(fn)>(fn)]<class Tp>(const Tp & a,const Tp & b,bool reverse){
            return reverse ? f(b,a) : f(a,b);
        };
    }

    auto wrap_inject(auto && fn){
        return [f = std::forward<decltype(fn)>(fn)](){
            if constexpr(!std::is_same_v<std::decay_t<decltype(f)>,std::nullptr_t>){
                f();
            }
        };
    }

    /// @brief 基础的插入排序，时间复杂度O(n^2)   Tier: D
    /// @param begin   开始
    /// @param end     结束
    /// @param compare 比较函数 
    /// @param reverse 是否反转排序方向
    /// @param inject  每次进行一次数据交换后进行的操作
    template<class IterType,class CompareFn,class InjectFn = std::nullptr_t> 
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
    
        for(IterType current = (begin + 1);current < end;++current){
            value_t val = std::move(*current);
            IterType j = current;

            while(j > begin && compare(*(j-1),val,reverse)){
                *j = std::move(*(j-1));
                inject();
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
    template<class IterType,class CompareFn,class InjectFn = std::nullptr_t> 
    void bubble(
        IterType begin,
        IterType end,
        CompareFn&& i_compare,
        bool reverse = false,
        InjectFn && i_inject = nullptr
    ){
        using value_t = std::iterator_traits<IterType>::value_type;
        auto compare = wrap_compare(std::forward<CompareFn>(i_compare));
        auto inject = wrap_inject(std::forward<InjectFn>(i_inject));
    
        for(IterType current = begin;current < (end - 1);++current){
            for(IterType i_current = begin;i_current < (end - 1 - (current - begin));++i_current){
                if(compare(*i_current,*(i_current+1),reverse)){
                    std::swap(*i_current,*(i_current+1));
                    inject();
                }
            }
        }
    }

    /// @brief 快排Lomuto分区方式
    struct PartitionLomuto {
        template<class IterType,class CompareFn,class InjectFn,class Node> 
            static void partition(IterType low,IterType high,
                CompareFn& compare,bool reverse,InjectFn & inject,
                std::vector<Node> & nodes
            ){
                IterType i = low;

                for(IterType cur = low;cur < high;++cur){
                    if(compare(*high,*cur,reverse)){
                        if(i != cur){
                            std::swap(*i,*cur);
                            inject();
                        }
                        ++i;
                    }
                }
                std::swap(*i,*high);
                inject();
                
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
        template<class IterType, class CompareFn, class InjectFn, class Node>
        static void partition(IterType low, IterType high,
                            CompareFn& compare, bool reverse, InjectFn& inject,
                            std::vector<Node>& nodes) {
            using value_t = typename std::iterator_traits<IterType>::value_type;
            IterType pivot = (low + (high - low) / 2);

            IterType l = low;
            IterType h = high;

            while(l <= h){
                while(compare(*pivot,*l,reverse)) ++l;
                while(compare(*h,*pivot,reverse)) --h;
                
                if(l <= h){
                    if(l != h){
                        [[unlikely]] if(l == pivot){
                            pivot = h;
                        }else [[unlikely]] if(h == pivot){
                            pivot = l;
                        }

                        std::swap(*l, *h);
                        inject();
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
        template<class IterType, class CompareFn, class InjectFn, class Node>
        static void partition(IterType low, IterType high,
                            CompareFn& compare, bool reverse, InjectFn& inject,
                            std::vector<Node>& nodes) {
            IterType pivot = (low + (high - low) / 2);

            // pivot > low ?
            bool c1 = compare(*pivot,*low,false);
            // pivot < high ?
            bool c2 = compare(*high,*pivot,false);
        
            /// @TODO 完成这个
        }
    };

    /// @brief 基础的快速排序，时间复杂度O(nlogn) Tier:B
    /// @param begin   开始
    /// @param end     结束
    /// @param compare 比较函数 
    /// @param reverse 是否反转排序方向
    /// @param inject  每次进行一次数据交换后进行的操作
    template<class PartitionMethod = PartitionLomuto,class IterType,
            class CompareFn,class InjectFn = std::nullptr_t
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
                        reverse,inject,stack);
        }
    }
}

 #endif