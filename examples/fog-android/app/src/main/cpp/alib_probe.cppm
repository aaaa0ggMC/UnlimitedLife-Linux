// alib_probe.cppm —— alib6 各模块 Android 综合探测（目前先引入 alib）
//
// 每个被测模块一组 try/catch 探测，结果累积成可读报告，
// 经 extern "C" alib_probe_cstr() 返回给 JNI 桥（TU 拆分，理由同
// AndroidJniGccModules：import std 的 TU 不能 include 平台 C 头）。
export module alib_probe;

import std;
import alib6;

// 注意：不能放匿名命名空间——导出 inline 函数体内不得使用内部链接实体

void probe_line(std::string& out, std::string_view module, std::string_view body) {
    std::format_to(std::back_inserter(out), "[{}] {}\n", module, body);
}

/// 以 PASS/FAIL 包裹执行，异常不外泄
void probe(std::string& out, std::string_view module, auto&& fn) {
    try {
        std::string line = fn();
        probe_line(out, module, "PASS " + line);
    } catch(const std::exception& e) {
        probe_line(out, module, std::string("FAIL ") + e.what());
    } catch(...) {
        probe_line(out, module, "FAIL <non-std exception>");
    }
}

export inline std::string alib_probe_text() {
    std::string out;
    out.reserve(4096);

    // ---- core.types ----
    probe(out, "core.types", [] {
        alib6::u32 a = 40 + 2;
        alib6::i64 b = -20260929;
        return std::format("u32={} i64={} sizeof(usize)={}", a, b, sizeof(alib6::usize));
    });

    // ---- core.str ----
    probe(out, "core.str", [] {
        auto s = alib6::ext::to_string(12345);
        return std::format("to_string(12345)={} len={}", std::string_view(s), s.size());
    });

    // ---- clock ----
    probe(out, "clock", [] {
        alib6::Clock clock;
        auto [a, b] = clock.now();
        return std::format("Clock.now() = ({:.6f}, {:.6f})", a, b);
    });

    // ---- core.io ----
    probe(out, "core.io", [] {
        auto entry = alib6::io::load_entry("/proc/version");
        if(entry.invalid()) return std::string("load_entry(/proc/version) invalid");
        std::string content;
        const auto size = entry.read(content);
        if(size == std::numeric_limits<std::size_t>::max()) {
            return std::string("read failed");
        }
        auto nl = content.find('\n');
        return std::format("read {} bytes: {}", size,
                           content.substr(0, nl == std::string::npos ? 40 : nl));
    });

    // ---- core.storage ----
    probe(out, "core.storage", [] {
        alib6::storage::MonoBitSet bits(128);
        bits.set(3);
        bits.set(70);
        bits.set(127);
        return std::format("MonoBitSet(128) set {{3,70,127}} -> test(3)={} test(70)={} test(127)={}",
                           bits.test(3), bits.test(70), bits.test(127));
    });

    // ---- core.error ----
    probe(out, "core.error", [] {
        alib6::Error err;
        alib6::ErrorWrapper ew(err);
        ew.report("probe: intentional error report");
        bool has = err.has_error();
        return std::format("report -> has_error={}", has);
    });

    // ---- log + log.prefab ----
    probe(out, "log", [] {
        alib6::Logger logger;
        logger.append_mod<alib6::lot::Console>("console");
        alib6::LogFactory fac(logger, "probe");
        fac(alib6::Severity::Info) << "alib6 log pipeline ok" << std::endl;
        logger.flush();

        std::pmr::string target(std::pmr::get_default_resource());
        alib6::log::write_to_log(target, std::make_pair(3, 4));          // pair 重载
        alib6::log::write_to_log(target, std::optional<int>{7});          // optional 重载
        alib6::log::write_to_log(target, std::chrono::microseconds(5));   // duration 重载
        return std::format("Console+LogFactory ok; write_to_log: {}",
                           std::string_view(target));
    });

    // ---- ecs ----
    probe(out, "ecs", [] {
        struct Position { int x{0}, y{0}; };
        alib6::ecs::EntityManager em;
        auto e1 = em.create_wrapper();
        auto e2 = em.create_wrapper();
        e1.add<Position>(Position{1, 2});
        e2.add<Position>(Position{3, 4});
        int sum = 0, count = 0;
        em.view<Position>().for_each([&](Position& p) { sum += p.x + p.y; ++count; });
        return std::format("2 entities + Position view -> count={} sum={}", count, sum);
    });

    // ---- data ----
    probe(out, "data", [] {
        alib6::AData data;
        data["name"] = "fog-android";
        data["count"] = 7;
        return std::format("AData dump: {}", data.str(alib6::data::Flat()));
    });

    // ---- table ----
    probe(out, "table", [] {
        alib6::table::Table tbl;
        tbl[0][0] << "alib6";
        tbl[1][0] << "table";
        return std::format("Table 2x1 cells written");
    });

    // ---- perf ----
    probe(out, "perf", [] {
        alib6::perf::Benchmark bench([] { return 1 + 1; });
        auto results = bench.run(50, 3);
        auto info = results.calculate();
        return std::format("Benchmark(lambda).run(50,3) -> avg {:.4f} ms, cv {:.2f}%",
                           info.global_aver, info.cv);
    });

    std::format_to(std::back_inserter(out),
                   "\n== alib6 on Android: {}/11 modules probed ==",
                   std::count(out.begin(), out.end(), '\n'));
    return out;
}

// extern "C" 出口：不 export、不加 inline（符号必须真正产出），
// 供 JNI 桥（纯 TU）链接调用。
extern "C" const char* alib_probe_cstr() {
    static const std::string cached = alib_probe_text();
    return cached.c_str();
}
