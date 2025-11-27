/**
 * @file linear_storage.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 线性存储类，目前使用vector，后期可以变成sparse_set啥的
 * @version 0.1
 * @date 2025/11/27
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/11/27 
 */
#ifndef AECS_LINEAR_STORAGE_H_INCLUDED
#define AECS_LINEAR_STORAGE_H_INCLUDED
#include <alib-g3/autil.h>
#include <alib-g3/adebug.h>
#include <limits>
#include <vector>

namespace alib::g3::ecs::detail{
    /// @brief 是否可以通过reset方法实现建议重构？
    template<class T,class... Args> concept CanReset = requires(T & t,Args&&... args){
        t.reset(std::forward<Args>(args)...);
    };
    /// @brief 一个合格的component必须要保证有reset方法，并且reset和构造函数的初始化列表建议一致！
    template<class T,class... Args> concept IsResetableComponent = requires(T & t,Args&&... args){
        t.reset(std::forward<Args>(args)...);
        T(std::forward<Args>(args)...);
    };

    /// @brief 单调递增的bitset
    struct DLL_EXPORT MonoticBitSet{
        /// @brief 单条数据类型
        using store_t = uint64_t;
        /// @brief 单条数据的大小
        constexpr static size_t data_size = sizeof(store_t) * __CHAR_BIT__;
        /// @brief 数据集
        std::vector<store_t> mask; 

        /// @brief 获取对应index对应的第几个store_t变量
        /// @param count 元素index
        /// @return mask的index
        inline size_t get_ecount(size_t count){
            size_t ecount = count / data_size + 1;
            return ecount;
        }

        /// @brief 确保数据集里面支持这么多元素
        /// @param count 元素数量
        inline void ensure(size_t count){
            size_t ecount = get_ecount(count);

            if(ecount > mask.size()){
                mask.resize(ecount,0);
            }
        }

        inline void set(size_t pos){
            panic_debug(get_ecount(pos) > mask.size(),"Array out of bounds!");
            size_t base = pos / data_size;
            size_t offset = pos % data_size;

            mask[base] |= ((store_t)1 << offset);
        }

        inline void reset(size_t pos){
            panic_debug(get_ecount(pos) > mask.size(),"Array out of bounds!");
            size_t base = pos / data_size;
            size_t offset = pos % data_size;

            mask[base] &= ~((store_t)1 << offset);
        }


        inline bool get(size_t pos){
            panic_debug(get_ecount(pos) > mask.size(),"Array out of bounds!");
            size_t base = pos / data_size;
            size_t offset = pos % data_size;

            return (mask[base] >> offset) & 0x1;
        }

        inline void for_each(auto && func,size_t max_elements = SIZE_MAX){
            size_t all = 0;
            for(auto t : mask){
                for(unsigned int i = 0;i < data_size;++i){
                    if(all >= max_elements)return;
                    func(all,(t >> i) & 0x1);
                    ++all;
                }
            }
        }

        inline void for_each_skip_0_bits(auto && func,size_t max_elements = SIZE_MAX){
            size_t all = 0;
            for(auto t : mask){
                if(!t){
                    all += data_size;
                    continue;
                }
                for(unsigned int i = 0;i < data_size;++i){
                    if(all >= max_elements)return;
                    if((t >> i) & 0x1)func(all);
                    ++all;
                }
            }
        }

        inline void for_each_skip_1_bits(auto && func,size_t max_elements = SIZE_MAX){
            size_t all = 0;
            for(auto t : mask){
                if(t == std::numeric_limits<store_t>::max()){
                    all += data_size;
                    continue;
                }
                for(unsigned int i = 0;i < data_size;++i){
                    if(all >= max_elements)return;
                    if((~(t >> i) & 0x1))func(all);
                    ++all;
                }
            }
        }

        inline void clear(bool val = false){
            store_t fill_num = val?std::numeric_limits<store_t>::max():0;
            std::fill(mask.begin(),mask.end(),fill_num);
        }

        inline MonoticBitSet(size_t elements = 0){
            ensure(elements);
        }
    };

    template<class T> struct DLL_EXPORT LinearStorage{
        std::vector<T>         data;
        std::vector<size_t>     free_elements;
        MonoticBitSet          available_bits;

        // 支持ref直接引用
        using reference = T&;
        using value_type = T;
        inline reference operator[](size_t index){
            return data[index];
        }
        inline size_t size(){return data.size();}
        inline bool empty(){return data.empty();}

        inline LinearStorage(size_t reserve_size = 0){
            data.reserve(reserve_size);
        }

        template<class...Ts> inline T& next(Ts&&... args){
            available_bits.ensure(data.size() + 1);
            return data.emplace_back(std::forward<Ts>(args)...);
        }

        // make sure you have check that there are free_one
        template<class...Ts> inline T& next_free(Ts&&... args){
            size_t index = 0;
            return next_free_with_index(index,std::forward<Ts>(args)...);
        }

        template<class...Ts> inline T& next_free_with_index(size_t& i_index,Ts&&... args){
            panic_debug(free_elements.empty(),"There are no free elements in storage!");
            size_t index = free_elements.back();
            available_bits.reset(index);
            free_elements.pop_back();

            i_index = index;

            T & ret = data[index];
            if constexpr(CanReset<T,Ts...>){
                // 这一块暂时按下不表，后面需要的时候再适配看看
                // static_assert(IsResetableComponent<T,Ts...>,"Reset function must have the same argument list with the constructor!");
                ret.reset(std::forward<Ts>(args)...);
            }else{
                ret.~T();
                new (&ret) T(std::forward<Ts>(args)...);
            }
            return ret;
        }

        inline void remove(size_t index){
            available_bits.set(index);
            free_elements.push_back(index);
        }

        template<class... Ts> inline T& try_next(bool & flag,Ts&&... args){
            size_t index = 0;
            return try_next_with_index(flag,index,std::forward<Ts>(args)...);
        }

        template<class... Ts> inline T& try_next_with_index(bool & flag,size_t& index,Ts&&... args){
            if(free_elements.empty()){
                flag = true;
                index = data.size();
                return next(std::forward<Ts>(args)...);
            }else {
                flag = false;
                return next_free_with_index(index,std::forward<Ts>(args)...);
            }
        }

    };
}


#endif