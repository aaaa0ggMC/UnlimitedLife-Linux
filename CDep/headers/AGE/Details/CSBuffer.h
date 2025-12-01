#ifndef AGE_CSBUFFER_H_INCLUDED
#define AGE_CSBUFFER_H_INCLUDED
#include <memory_resource>
#include <string>
#include <unordered_set>

namespace age::detail {
    struct ConstantStringBuffer{
        std::pmr::unsynchronized_pool_resource upstreamPool;
        // 只能分配不能释放，对于短期字符串不适合存储
        std::pmr::monotonic_buffer_resource poolBuffer { &upstreamPool };
        std::pmr::polymorphic_allocator<char> allocator { &poolBuffer };
        std::pmr::unordered_set<std::pmr::string> pool{0, std::hash<std::pmr::string>{}, std::equal_to<std::pmr::string>{}, allocator};

        std::string_view get(std::string_view str){
            auto [it, inserted] = pool.emplace(std::pmr::string(str, allocator));
            return *it;
        }
    };
}

#endif