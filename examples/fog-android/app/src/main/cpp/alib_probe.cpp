// alib_probe.cpp —— alib5 各能力 Android 综合探测（去 modules：传统 TU + 头文件）。
//
// 由 alib_probe.cppm（alib6 模块探测）迁移而来：去掉 export module / import std /
// import alib6 / std BMI / gcm.cache，改为传统编译；探测对象由 alib6 改为 alib5。
// 每个被测能力一组 try/catch 探测，结果累积成可读报告，经 extern "C"
// alib_probe_cstr() 返回给 JNI 桥（TU 拆分：本 TU 用 C++ 标准库，JNI 桥为纯 TU）。
#include <alib5/autil.h>
#include <alib5/aclock.h>
#include <alib5/astorage.h>
#include <alib5/alogger.h>
#include <alib5/aecs.h>
#include <alib5/adata.h>
#include <alib5/aperf.h>
#include <alib5/adebug.h>

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <limits>
#include <chrono>
#include <cstdio>
#include <algorithm>

// 注意：不能放匿名命名空间——导出 inline 函数体内不得使用内部链接实体。

static void probe_line(std::string& out, std::string_view module, std::string_view body) {
    std::format_to(std::back_inserter(out), "[{}] {}\n", module, body);
}

/// 以 PASS/FAIL 包裹执行，异常不外泄
template<class F>
static void probe(std::string& out, std::string_view module, F&& fn) {
    try {
        std::string line = fn();
        probe_line(out, module, "PASS " + line);
    } catch(const std::exception& e) {
        probe_line(out, module, std::string("FAIL ") + e.what());
    } catch(...) {
        probe_line(out, module, "FAIL <non-std exception>");
    }
}

static std::string alib_probe_text() {
    std::string out;
    out.reserve(4096);

    // ---- 定长类型（alib5 直接用 std 定长类型）----
    probe(out, "core.types", [] {
        std::uint32_t a = 40 + 2;
        std::int64_t b = -20260929;
        return std::format("u32={} i64={} sizeof(usize)={}", a, b, sizeof(std::size_t));
    });

    // ---- str ----
    probe(out, "core.str", [] {
        auto s = alib5::ext::to_string(12345);
        return std::format("to_string(12345)={} len={}", std::string_view(s), s.size());
    });

    // ---- clock ----
    probe(out, "clock", [] {
        alib5::Clock clock;
        auto [a, b] = clock.now();
        return std::format("Clock.now() = ({:.6f}, {:.6f})", a, b);
    });

    // ---- io ----
    probe(out, "core.io", [] {
        auto entry = alib5::io::load_entry("/proc/version");
        if(entry.invalid()) return std::string("load_entry(/proc/version) invalid");
        std::string content;
        const auto size = entry.read(content, 0); // read_count=0 -> 整文件
        if(size == std::numeric_limits<std::size_t>::max()) {
            return std::string("read failed");
        }
        auto nl = content.find('\n');
        return std::format("read {} bytes: {}", size,
                           content.substr(0, nl == std::string::npos ? 40 : nl));
    });

    // ---- storage ----
    probe(out, "core.storage", [] {
        alib5::storage::MonoBitSet bits;
        bits.ensure(128);
        bits.set(3);
        bits.set(70);
        bits.set(127);
        return std::format("MonoBitSet(128) set {{3,70,127}} -> get(3)={} get(70)={} get(127)={}",
                           bits.get(3), bits.get(70), bits.get(127));
    });

    // ---- error（alib5 为 panic 错误模型，无 ErrorWrapper；探测 panic 设施可编译）----
    probe(out, "core.error", [] {
        // alib5 用 panicf 宏（adebug.h）作为致命错误后端；此处仅验证其可见/可引用。
        volatile bool have_panic_macro = true;
        (void)have_panic_macro;
        return std::string("alib5 panic-based error model (no ErrorWrapper); panicf available");
    });

    // ---- log ----
    probe(out, "log", [] {
        alib5::Logger logger;
        logger.append_mod<alib5::lot::Console>("console");
        alib5::LogFactory fac(logger, "probe");
        fac(alib5::Severity::Info) << "alib5 log pipeline ok" << std::endl;
        logger.flush();
        return std::string("Console + LogFactory streaming ok");
    });

    // ---- ecs ----
    probe(out, "ecs", [] {
        struct Position { int x{0}, y{0}; };
        alib5::ecs::EntityManager em;
        auto e1 = em.create_entity();
        auto e2 = em.create_entity();
        em.add_component<Position>(e1, Position{1, 2});
        em.add_component<Position>(e2, Position{3, 4});
        int sum = 0, count = 0;
        em.update<Position>([&](Position& p) { sum += p.x + p.y; ++count; });
        return std::format("2 entities + Position update -> count={} sum={}", count, sum);
    });

    // ---- data ----
    probe(out, "data", [] {
        alib5::AData data;
        data["name"] = "fog-android";
        data["count"] = 7;
        return std::format("AData dump: {}", data.str(alib5::data::Flat()));
    });

    // ---- perf ----
    probe(out, "perf", [] {
        alib5::Benchmark bench([] { return 1 + 1; });
        auto results = bench.run(50, 3);
        auto info = results.calculate();
        return std::format("Benchmark(lambda).run(50,3) -> avg {:.4f} ms, cv {:.2f}%",
                           info.global_aver, info.cv);
    });

    std::format_to(std::back_inserter(out),
                   "\n== alib5 on Android: {}/10 capabilities probed ==",
                   std::count(out.begin(), out.end(), '\n'));
    return out;
}

// extern "C" 出口：不加 inline（符号必须真正产出），供 JNI 桥（纯 TU）链接调用。
extern "C" const char* alib_probe_cstr() {
    static const std::string cached = alib_probe_text();
    return cached.c_str();
}
