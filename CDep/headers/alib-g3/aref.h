/**
 * @file aref.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 不会悬垂的比较安全的容器数据wrapper,Release下单次性能损失为0.3ns
 * @version 0.1
 * @date 2025/11/17
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 ********************************************************
    @note Benchmark测试(使用aperf.h)平台plat[3](见SUMMARY.md):
    @par 单引用：
    @code {.cpp}
    std::vector<int> a = {1,2,3};
    auto& vana_ref = a[0];
    auto new_ref = ref(a,0);
    lg(LOG_INFO) << Benchmark([&vana_ref]{
        vana_ref = 0;
    }).run(10'000,10'000) << endlog;
    lg(LOG_INFO) << Benchmark([&new_ref]{
       new_ref = 0;
    }).run(10'000,10'000) << endlog;
    @endcode
DEBUG:
    C++ Ref 1.410609797858342ns
    Wrapper 8.927001545089297ns
RELEASE:
    C++ Ref 1.2750915630022064ns
    Wrapper 1.5495357956751832ns

    @par 多引用
    @code {.cpp}
    std::vector<std::vector<int>> a = {
        {1,2,3,4},
        {5,6,7},
        {8,9}
    };
    auto& vana_ref = a[0][1];
    auto new_ref = refs(a,0,1);

    lg(LOG_INFO) << Benchmark([&vana_ref]{
        vana_ref = 0;
    }).run(10'000,10'000) << endlog;
    lg(LOG_INFO) << Benchmark([&new_ref]{
        new_ref = 0;
    }).run(10'000,10'000) << endlog;
    lg(LOG_INFO) << Benchmark([&a]{
        a[0][1] = 0;
    }).run(10'000,10'000) << endlog;
    @endcode
DEBUG:
    C++ Ref 1.494575940341747ns
    Wrapper 19.85820199479349ns
    RawCall 6.377627414622111ns
RELEASE:
    C++ Ref 1.2744976629619487ns
    Wrapper 1.9192375475540757ns
    RawCall 1.395340859744465ns

    @par 结论
    更多测试可自行，从结果来看，DEBUG模式下单次cost为8ns左右，RELEASE下为0.4ns左右，对于多引用，性能损失稳定为N*Base
    可用且安全
********************************************************
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
        t.size();
        t.empty();
        typename T::value_type;
        typename T::reference;
    };

    template<class T> concept IsNumber =
        std::is_same_v<T,size_t> ||
        std::is_same_v<T,int> ||
        std::is_same_v<T,long> ||
        std::is_same_v<T,unsigned int> ||
        std::is_same_v<T,unsigned long> ||
        std::is_same_v<T,short> ||
        std::is_same_v<T,unsigned short>;


    /// @brief 通过持有index从而保证一定的安全性，Release优化后仅仅多出0.3ns(5.7Ghz电脑)的解引用时长
    /// @tparam Cont 
    template<CanAccessItem Cont> struct RefWrapper{
        using BaseType = typename Cont::value_type;
        /// 容器的引用，所以你需要保证至少容器是存在的，嵌套容器就可能不太支持了
        Cont & cont;
        /// index
        size_t index;

        /// 确认现在是否还存在数据
        inline bool has_data(){
            return index < cont.size();
        }
    
        /// 获取当前的引用
        inline auto& get(){
            // debug下防止shrink
            assert(index < cont.size());
            return cont[index];
        }

        /// 访问应用内部
        inline auto* operator->(){
            return &(get());
        }

        /// 尝试调用引用的赋值
        template<class U> inline auto operator=(U&& val) 
            -> decltype(cont[index] = std::forward<U>(val))
        {
            return get() = std::forward<U>(val);
        }

        /// 增加index
        inline RefWrapper<Cont> operator+(int offset){
            auto t = *this;
            t.set_index(index + offset);
            return t;
        }

        /// 减少index
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

        /// 手动设置index
        inline RefWrapper<Cont>& set_index(size_t index){
            assert(index < cont.size());
            this->index = index;
            return *this;
        }

        /// 手动设置
        inline auto& operator()(size_t index){
            return set_index(index);
        }
    };

    template<CanAccessItem Cont> auto 
        ref(Cont & cont,size_t index){
        assert(index < cont.size());
        return RefWrapper<Cont>{cont,index};
    }

    /**
     * @brief 多层引用容器。
     * 
     * 用于在嵌套容器（如 vector<vector<T>>）中安全地引用多层元素，
     * 不会悬垂，但要求原始容器的生命周期有效。
     * 
     * @tparam Cont 容器类型
     * @tparam N 访问层数
     */
    template<CanAccessItem Cont,size_t N> struct MultiRefWrapper{
        using value_type = Cont::value_type;
        /// 底层容器，需要保证不悬垂
        Cont & cont;
        /// 存储索引的数组
        std::array<size_t,N> indices;
    private:

        /// 递归获取对象，每次都会进行一次检查
        template<CanAccessItem ICont,size_t Depth = 0> 
            auto& get_raw(ICont & ucont){
            assert(indices[Depth] < ucont.size());
            // 到达索引最内层，由于set_indices已经保证层数匹配
            if constexpr(Depth == N-1){
                return ucont[indices[Depth]];
            }else return get_raw<typename ICont::value_type,Depth+1>(ucont[indices[Depth]]); 
        }
        
        /// 最后一层调用
        template<CanAccessItem ICont,size_t Index = 0,IsNumber Arg> void 
            set_indices_raw(ICont& c,Arg&& index){ 
            assert(index < c.size());
            indices[Index] = std::forward<Arg>(index);   
        }

        /// 递归设置索引 每次都会进行一次检查
        template<CanAccessItem ICont,size_t Index = 0,IsNumber Arg,IsNumber... Args> void 
            set_indices_raw(ICont & c,Arg&& index,Args&&... iss){
            // 说明此时有附加参数但是没有对应层数
            static_assert(CanAccessItem<typename ICont::value_type>,"Too many indices! Your ref is too deep for the container!");
            assert(index < c.size());
            indices[Index] = std::forward<Arg>(index);
            set_indices_raw<typename ICont::value_type,Index+1,Args...>(c[index],std::forward<Args>(iss)...); // 递归检测
        }
        
    public:
        template<size_t N2> MultiRefWrapper(MultiRefWrapper<Cont,N2> & c1):cont(c1.cont){
            static_assert(N2 >= N,"Cannot lowercast the container!");
            std::copy(c1.indices.begin(),c1.indices.end(),indices.begin());
        }

        template<size_t dummy> MultiRefWrapper(MultiRefWrapper<Cont,dummy> & c1,size_t index):cont(c1.cont){
            static_assert(N == dummy+1,"Cannot uppercast the container!");
            std::copy(c1.indices.begin(),c1.indices.end(),indices.begin());
            
            assert(index < c1.get().size());
            indices[N-1] = index;
        }

        /// 构造引用对象
        template<IsNumber... Args> inline MultiRefWrapper(Cont & c,Args&&... val):cont(c){
            set_indices(std::forward<Args>(val)...);
        }

        /// 获取内存对象
        inline auto& get(){
            return get_raw<Cont,0>(cont);
        }

        /// 设置索引
        template<IsNumber... Args> inline void 
            set_indices(Args&&... iss){
            set_indices_raw<Cont,0,Args...>(cont,std::forward<Args>(iss)...); // 递归检测
        }

        /// 获取当前对象
        inline auto* operator->(){
            return &get();
        }

        /// 往上面走一层
        inline auto operator()(){
            static_assert(N != 1,"You have reached to the root of the container!");
            return MultiRefWrapper<Cont,N-1>(*this);
        }

        /// 设置当前层索引
        inline auto operator()(size_t index){
            MultiRefWrapper<Cont,N> ret(*this);
            ret.indices[N-1] = index;
            // 安全检测在get里面做
            return ret;
        }

        /// 往下走一层
        inline auto operator[](size_t index){
            static_assert(CanAccessItem<value_type>,"Too many indices! Your ref is too deep for the container!");
            return MultiRefWrapper<Cont,N+1>(*this,index);
        }

                /// 尝试调用引用的赋值
        template<class U> inline auto operator=(U&& val) 
            -> decltype(get() = std::forward<U>(val))
        {
            return get() = std::forward<U>(val);
        }
    };

    /// 获取多层container的引用
    template<CanAccessItem Cont,IsNumber... Args> auto
        refs(Cont & cont,Args&&... indices){
        return MultiRefWrapper<Cont,sizeof...(Args)>(cont,std::forward<Args>(indices)...);
    }
}

#endif