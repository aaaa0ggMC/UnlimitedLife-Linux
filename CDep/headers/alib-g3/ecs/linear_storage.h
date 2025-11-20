#ifndef AECS_LINEAR_STORAGE_H_INCLUDED
#define AECS_LINEAR_STORAGE_H_INCLUDED
#include <alib-g3/autil.h>
#include <alib-g3/adebug.h>
#include <limits>
#include <vector>

namespace alib::g3::ecs::detail{
    template<class T,class... Args> concept CanReset = requires(T & t,Args&&... args){
        t.reset(std::forward<Args>(args)...);
    };
    /// 一个合格的component必须要保证有reset方法，并且reset和构造函数的初始化列表一致！
    template<class T,class... Args> concept IsResetableComponent = requires(T & t,Args&&... args){
        t.reset(std::forward<Args>(args)...);
        T(std::forward<Args>(args)...);
    };

    struct DLL_EXPORT MonoticBitSet{
        using store_t = uint64_t;
        constexpr static size_t data_size = sizeof(store_t) * __CHAR_BIT__;
        std::vector<store_t> mask; 

        inline size_t get_ecount(size_t count){
            size_t ecount = count / data_size + 1;
            return ecount;
        }

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
                static_assert(IsResetableComponent<T,Ts...>,"Reset function must have the same argument list with the constructor!");
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