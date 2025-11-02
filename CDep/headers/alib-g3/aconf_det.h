#ifndef ACONF_DET_H
#define ACONF_DET_H
#include <alib-g3/autil.h>
#include <limits.h>

namespace alib::g3{
    /// 一个类是否可以被dump
    template<class T>
    concept CanDump = 
        requires(T & t,std::string_view sv){t += sv;} || 
        requires(T & t,std::string_view sv){t << sv;};

    template<class T>
    struct ConfigCastResult{
        std::optional<T> value {std::nullopt};
        long long errpos {-1};
    };


    /**
     * @brief 默认的转换类，目前支持int long short bool float double这几个基础类型
     * @tparam T 
     * @start-date 2025/11/02
     */
    struct DefConfigCaster{
        template<class T> static ConfigCastResult<T> try_cast(std::string_view);
    };

    // ---------------- long ----------------
    template<> inline ConfigCastResult<long> DefConfigCaster::try_cast(std::string_view s){
        char* endptr = nullptr;
        errno = 0;
        long val = std::strtol(s.data(), &endptr, 10);

        size_t offset = endptr ? static_cast<size_t>(endptr - s.data()) : 0;
        return { val, (long long)(offset < s.size() ? offset : static_cast<size_t>(-1)) };
    }

    // ---------------- int ----------------
    template<> inline ConfigCastResult<int> DefConfigCaster::try_cast(std::string_view s){
        auto r = try_cast<long>(s);
        if (r.value) {
            if (*r.value > INT_MAX) r.value = INT_MAX;
            if (*r.value < INT_MIN) r.value = INT_MIN;
        }
        return { r.value ? std::optional<int>{static_cast<int>(*r.value)} : std::nullopt, r.errpos };
    }

    // ---------------- short ----------------
    template<> inline ConfigCastResult<short> DefConfigCaster::try_cast(std::string_view s){
        auto r = try_cast<long>(s);
        if (r.value) {
            if (*r.value > SHRT_MAX) r.value = SHRT_MAX;
            if (*r.value < SHRT_MIN) r.value = SHRT_MIN;
        }
        return { r.value ? std::optional<short>{static_cast<short>(*r.value)} : std::nullopt, r.errpos };
    }

    // ---------------- bool ----------------
    template<> inline ConfigCastResult<bool> DefConfigCaster::try_cast(std::string_view s){
        if (s == "true" || s == "1") return { true, -1 };
        if (s == "false" || s == "0") return { false, -1};
        auto r = try_cast<long>(s);
        if (r.value) return { *r.value != 0, r.errpos };
        return { std::nullopt, 0 };
    }

    // ---------------- std::string_view ----------------
    template<> inline ConfigCastResult<std::string_view> DefConfigCaster::try_cast(std::string_view s){
        return { s, -1};
    }

    // ---------------- float ----------------
    template<> inline ConfigCastResult<float> DefConfigCaster::try_cast(std::string_view s){
        char* endptr = nullptr;
        errno = 0;
        float val = std::strtof(s.data(), &endptr);

        size_t offset = endptr ? static_cast<size_t>(endptr - s.data()) : 0;
        return { val, (long long)(offset < s.size() ? offset : static_cast<size_t>(-1)) };
    }

    // ---------------- double ----------------
    template<> inline ConfigCastResult<double> DefConfigCaster::try_cast(std::string_view s){
        char* endptr = nullptr;
        errno = 0;
        double val = std::strtod(s.data(), &endptr);

        size_t offset = endptr ? static_cast<size_t>(endptr - s.data()) : 0;
        return { val, (long long)(offset < s.size() ? offset : static_cast<size_t>(-1)) };
    }

    namespace detail {
        template<class T>
        void dump_append(T& target, std::string_view sv){
            if constexpr(requires{target += sv;}){
                target += sv;
            }else if constexpr(requires{target << sv;}){
                target << sv;
            }
        }
    }
}



#endif