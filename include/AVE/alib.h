/**
 * @file alib.h
 * @brief AVE 对外部基础库（alib5）与 std 的收敛包含层（去 modules 迁移）。
 *
 * 原 AVE 模块经 `import alib6; import std;` 获得定长类型/错误/IO/时钟/存储等；
 * 迁移后统一经本头引入 alib5（头文件式）+ AVE 自带的 error_code.h。
 * 仅收敛「外部依赖 + 类型 + 少量位图辅助」，不合并 AVE 各子系统公开 API（保留分层）。
 */
#pragma once

#include <AVE/error_code.h>

// alib5 头文件式设施（只读使用已安装/源码 alib5，不修改外部库）。
#include <alib5/autil.h>    // io::read_all / io::FileEntry / io::load_entry / str::StringPool
#include <alib5/aclock.h>   // Clock / RateLimiter
#include <alib5/astorage.h> // storage::MonoBitSet / storage::FreelistLinearStorage
#include <alib5/alogger.h>  // Severity / LogLevel / 日志

namespace ave {

    // AVE 原先用到的位图接口名与 alib5 MonoBitSet 不完全一致（get 而非 test，
    // 也没有 find_next_*）。这里用 alib5 原生 fill/none 与 for_each_* 提供薄辅助，
    // 保持 buffer/reflect 中 dirty-mask 的语义不变。
    using BitSet = alib5::storage::MonoBitSet;

    inline void bitset_clear(BitSet& bs) noexcept {
        bs.fill(false);
    }

    [[nodiscard]] inline bool bitset_none(const BitSet& bs) noexcept {
        return bs.none();
    }

    /// 首个置位且 >= from 的位置（遍历全部置位，调用方自行校验上界），等价原 find_next_1(from)。
    [[nodiscard]] inline std::optional<usize>
    bitset_find_next_1(const BitSet& bs, usize from) {
        std::optional<usize> r;
        bs.for_each_1([&](usize p) { if(p >= from && !r) r = p; });
        return r;
    }

    /// 首个未置位且 >= from 的位置（限制在 [0, count)），等价原 find_next_0(from, count)。
    [[nodiscard]] inline std::optional<usize>
    bitset_find_next_0(const BitSet& bs, usize from, usize count) {
        std::optional<usize> r;
        bs.for_each_0([&](usize p) { if(p >= from && !r) r = p; }, count);
        return r;
    }

    /// alib5 FreelistLinearStorage 无 occupied_count()；占用数 = 总槽位 - 空闲槽位。
    template<class T>
    [[nodiscard]] inline usize
    freelist_occupied_count(const alib5::storage::FreelistLinearStorage<T>& s) noexcept {
        return s.size() - s.free_elements.size();
    }

} // namespace ave
