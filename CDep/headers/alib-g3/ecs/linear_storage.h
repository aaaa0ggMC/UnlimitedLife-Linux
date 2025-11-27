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

        /// @brief 置位对应元素的位置
        inline void set(size_t pos){
            panic_debug(get_ecount(pos) > mask.size(),"Array out of bounds!");
            size_t base = pos / data_size;
            size_t offset = pos % data_size;

            mask[base] |= ((store_t)1 << offset);
        }

        /// @brief 重置对应位置的元素
        inline void reset(size_t pos){
            panic_debug(get_ecount(pos) > mask.size(),"Array out of bounds!");
            size_t base = pos / data_size;
            size_t offset = pos % data_size;

            mask[base] &= ~((store_t)1 << offset);
        }

        /// @brief 获取对应位置数据的内容
        /// @return 是否位置
        inline bool get(size_t pos){
            panic_debug(get_ecount(pos) > mask.size(),"Array out of bounds!");
            size_t base = pos / data_size;
            size_t offset = pos % data_size;

            return (mask[base] >> offset) & 0x1;
        }

        /// @brief 遍历
        /// @param func 相应函数，第一个为位置，第二个为位的状态
        /// @param max_elements 
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

        /// @brief 遍历，但是跳过为0的位
        /// @param func 相应函数，只需要一个参数（元素位置）
        /// @param max_elements 最大的遍历到的位置
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

        /// @brief 遍历，但是跳过为1的位
        /// @param func 相应函数，只需要一个参数（元素位置）
        /// @param max_elements 最大的遍历到的位置
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

        /// @brief 清除所有位为0/1
        /// @param val 值，默认false
        inline void clear(bool val = false){
            store_t fill_num = val?std::numeric_limits<store_t>::max():0;
            std::fill(mask.begin(),mask.end(),fill_num);
        }

        /// @brief 构造函数
        /// @param elements 初始化大小 
        inline MonoticBitSet(size_t elements = 0){
            ensure(elements);
        }
    };

    /**
     * @brief 线性存储的对象
     * @tparam T 存储的内部类型，建议为trival-copyable
     * @start-date 2025/11/27
     * @note 内部类型支持的注入有reset(...)，会在复用的时候调用，要是不存在reset会选择构造函数
     *  因此建议构造函数和reset签名一致，但是不是硬性要求
     */
    template<class T> struct DLL_EXPORT LinearStorage{
        /// @brief 目前的内部数据类型
        std::vector<T>         data;
        /// @brief 空闲数据块列表
        std::vector<size_t>     free_elements;
        /// @brief 用于遍历的列表
        MonoticBitSet          available_bits;

        //// 支持ref直接引用 ////
        /// 引用类型
        using reference = T&;
        /// 基础类型
        using value_type = T;
        /// 重载[]获取对应的引用
        inline reference operator[](size_t index){
            return data[index];
        }
        /// 获取当前的数据量
        inline size_t size(){return data.size();}
        /// 获取当前是否为空
        inline bool empty(){return data.empty();}

        /// @brief 构造函数
        /// @param reserve_size 预留大小 
        inline LinearStorage(size_t reserve_size = 0){
            data.reserve(reserve_size);
            available_bits.ensure(reserve_size);
        }

        /// @brief 获取下一个对象
        /// @param ...args 传入对应类型构造函数的参数
        /// @return 对象的引用
        template<class...Ts> inline T& next(Ts&&... args){
            available_bits.ensure(data.size() + 1);
            return data.emplace_back(std::forward<Ts>(args)...);
        }

        /// @brief 获取空闲列表的下一个对象，release不会检测是否非空因此你需要自己保证非空，建议try_next
        /// @param ...args reset/构造函数 的参数列表 
        /// @return 对应引用
        template<class...Ts> inline T& next_free(Ts&&... args){
            size_t index = 0;
            return next_free_with_index(index,std::forward<Ts>(args)...);
        }

        /// @brief 获取空闲列表的下一个对象，release不会检测是否非空因此你需要自己保证非空，建议try_next
        /// @param i_index 引用在线性存储中的序号
        /// @param ...args reset/构造函数 的参数列表 
        /// @return 对应引用
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

        /// @brief 移除对应序号的数据
        /// @param index 数据的位置序号
        inline void remove(size_t index){
            available_bits.set(index);
            free_elements.push_back(index);
        }

        /// @brief 尝试获取下一个对象，存在空闲对象使用next_free否则使用next
        /// @param flag true:新对象  false:重用对象
        /// @param ...args reset/构造函数 的参数列表 
        /// @return 对象引用
        template<class... Ts> inline T& try_next(bool & flag,Ts&&... args){
            size_t index = 0;
            return try_next_with_index(flag,index,std::forward<Ts>(args)...);
        }

        /// @brief 尝试获取下一个对象，存在空闲对象使用next_free否则使用next
        /// @param flag true:新对象  false:重用对象
        /// @param index 引用在线性存储中的序号
        /// @param ...args reset/构造函数 的参数列表 
        /// @return 对象引用
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