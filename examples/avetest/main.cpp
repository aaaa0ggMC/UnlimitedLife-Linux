#include <vulkan/vulkan.h>

import alib6;
import std;
import ave;


struct FPSDetective {
    alib6::u64 frames { 0 };
    alib6::Clock clock;
    std::deque<alib6::f64> timestamps;

    void next_frame() {
        ++frames;
        timestamps.emplace_back(clock.get_all());
    }

    void write_to_log(std::pmr::string & log) const {
        if(timestamps.empty()) {
            log.append("No frames recorded.");
            return;
        }

        std::vector<alib6::f64> deltas;
        deltas.reserve(timestamps.size());
        deltas.push_back(timestamps.front());
        for(size_t i = 1; i < timestamps.size(); ++i) {
            deltas.push_back(timestamps[i] - timestamps[i - 1]);
        }

        const alib6::f64 total_time_ms = clock.get_all();
        const alib6::f64 n = static_cast<alib6::f64>(deltas.size());
        const alib6::f64 avg_fps = total_time_ms > 0 ? (frames * 1000.0 / total_time_ms) : 0.0;
        const alib6::f64 avg_frame_time = frames > 0 ? (total_time_ms / frames) : 0.0;

        // 1. 分位数与极值（排序）
        std::vector<alib6::f64> sorted_deltas = deltas;
        std::sort(sorted_deltas.begin(), sorted_deltas.end());

        auto get_percentile = [&](alib6::f64 pct) -> alib6::f64 {
            size_t idx = static_cast<size_t>(std::clamp<alib6::f64>(pct * (n - 1.0), 0.0, n - 1.0));
            return sorted_deltas[idx];
        };

        const alib6::f64 min_ft = sorted_deltas.front();
        const alib6::f64 max_ft = sorted_deltas.back();
        const alib6::f64 p25 = get_percentile(0.25);
        const alib6::f64 p50 = get_percentile(0.50); // Median
        const alib6::f64 p75 = get_percentile(0.75);
        const alib6::f64 p90 = get_percentile(0.90);
        const alib6::f64 p95 = get_percentile(0.95);
        const alib6::f64 p99 = get_percentile(0.99);
        const alib6::f64 iqr = p75 - p25;

        // 2. 1% Low & 0.1% Low FPS (基于最慢 1% 和 0.1% 帧平均帧时间)
        auto calc_low = [&](double pct) -> double {
            size_t count = std::max<size_t>(1, static_cast<size_t>(std::ceil(sorted_deltas.size() * pct)));
            count = std::min(count, sorted_deltas.size());
            double sum = 0.0;
            for(size_t i = sorted_deltas.size() - count; i < sorted_deltas.size(); ++i) {
                sum += sorted_deltas[i];
            }
            double avg = sum / count;
            return avg > 0.0 ? (1000.0 / avg) : 0.0;
        };
        const double low_1pct = calc_low(0.01);
        const double low_01pct = calc_low(0.001);

        // 3. 高阶统计矩（Moments: 方差、标准差、偏度 Skewness、超额峰度 Kurtosis）
        alib6::f64 sum_sq_diff = 0.0;
        alib6::f64 sum_cube_diff = 0.0;
        alib6::f64 sum_quad_diff = 0.0;
        for(auto dt : deltas) {
            auto diff = dt - avg_frame_time;
            auto d2 = diff * diff;
            sum_sq_diff += d2;
            sum_cube_diff += d2 * diff;
            sum_quad_diff += d2 * d2;
        }

        const alib6::f64 variance = deltas.size() > 1 ? (sum_sq_diff / (deltas.size() - 1)) : 0.0;
        const alib6::f64 stddev = std::sqrt(variance);
        const alib6::f64 cv = avg_frame_time > 0.0 ? (stddev / avg_frame_time * 100.0) : 0.0;

        alib6::f64 skewness = 0.0;
        alib6::f64 kurtosis = 0.0;
        if(stddev > 1e-9) {
            alib6::f64 sigma3 = stddev * stddev * stddev;
            alib6::f64 sigma4 = sigma3 * stddev;
            skewness = (sum_cube_diff / n) / sigma3;
            kurtosis = (sum_quad_diff / n) / sigma4 - 3.0; // Excess Kurtosis
        }

        // 4. 一阶差分相邻帧抖动 (Inter-frame Jitter) 与卡顿检测
        alib6::f64 sum_jitter = 0.0;
        alib6::f64 max_jitter = 0.0;
        alib6::u64 stutter_count = 0;
        for(size_t i = 1; i < deltas.size(); ++i) {
            alib6::f64 j = std::abs(deltas[i] - deltas[i - 1]);
            sum_jitter += j;
            if(j > max_jitter) max_jitter = j;
            if(deltas[i] > 1.5 * p50) ++stutter_count;
        }
        const alib6::f64 avg_jitter = deltas.size() > 1 ? (sum_jitter / (deltas.size() - 1)) : 0.0;
        const alib6::f64 smoothness = n > 0 ? (100.0 - (stutter_count * 100.0 / n)) : 100.0;

        // 5. 帧时间直方图 (Histogram)
        constexpr size_t bucket_count = 5;
        std::array<size_t, bucket_count> buckets {};
        const alib6::f64 hist_max = std::min(max_ft, p99 * 1.5 + 0.1);
        const alib6::f64 bucket_step = (hist_max > min_ft) ? ((hist_max - min_ft) / (bucket_count - 1)) : 0.0;

        for(auto dt : deltas) {
            if(bucket_step <= 0.0) {
                buckets[0]++;
                continue;
            }
            size_t b = static_cast<size_t>((dt - min_ft) / bucket_step);
            if(b >= bucket_count - 1) b = bucket_count - 1;
            buckets[b]++;
        }
        size_t max_bucket = 1;
        for(auto b : buckets) if(b > max_bucket) max_bucket = b;

        std::format_to(
            std::back_inserter(log),
            "\n"
            "====================== [ FPS Detective Report ] ======================\n"
            "  [ Overview ]\n"
            "    Total Frames     : {} frames\n"
            "    Duration         : {:.2f} s ({:.2f} ms)\n"
            "    Average FPS      : {:.2f} FPS  (Mean FT: {:.3f} ms)\n"
            "    Median FPS       : {:.2f} FPS  (P50 FT : {:.3f} ms)\n"
            "    1% Low FPS       : {:.2f} FPS\n"
            "    0.1% Low FPS     : {:.2f} FPS\n"
            "\n"
            "  [ Quantiles & Latency Spread ]\n"
            "    Min FT / Max FT  : {:.3f} ms / {:.3f} ms\n"
            "    P25 / P50 / P75  : {:.3f} ms / {:.3f} ms / {:.3f} ms\n"
            "    P90 / P95 / P99  : {:.3f} ms / {:.3f} ms / {:.3f} ms\n"
            "    IQR (P75 - P25)  : {:.3f} ms (Interquartile Range)\n"
            "\n"
            "  [ Statistical Moments & Distribution Shape ]\n"
            "    Stddev / Jitter  : {:.3f} ms (CV: {:.2f}%)\n"
            "    Inter-frame Diff : avg {:.3f} ms | max spike {:.3f} ms\n"
            "    Skewness (偏度)  : {:+.2f} ({})\n"
            "    Excess Kurtosis  : {:+.2f} ({})\n"
            "\n"
            "  [ Pacing & Smoothness ]\n"
            "    Stutter Frames   : {} frames ({:.2f}% > 1.5x Median)\n"
            "    Smoothness Score : {:.2f}%\n"
            "\n"
            "  [ Frame Time Distribution Histogram ]\n",
            frames,
            total_time_ms / 1000.0, total_time_ms,
            avg_fps, avg_frame_time,
            p50 > 0.0 ? (1000.0 / p50) : 0.0, p50,
            low_1pct,
            low_01pct,
            min_ft, max_ft,
            p25, p50, p75,
            p90, p95, p99,
            iqr,
            stddev, cv,
            avg_jitter, max_jitter,
            skewness, (skewness > 0.5 ? "Right-tailed / Stutter spikes" : (skewness < -0.5 ? "Left-tailed" : "Symmetric")),
            kurtosis, (kurtosis > 1.0 ? "Leptokurtic / Fat-tailed outliers" : (kurtosis < -1.0 ? "Platykurtic / Uniform" : "Mesokurtic / Normal")),
            stutter_count, (stutter_count * 100.0 / n),
            smoothness
        );

        constexpr size_t max_bar_width = 24;
        for(size_t i = 0; i < bucket_count; ++i) {
            alib6::f64 b_start = min_ft + i * bucket_step;
            alib6::f64 b_end = (i == bucket_count - 1) ? max_ft : (min_ft + (i + 1) * bucket_step);
            double pct = n > 0 ? (buckets[i] * 100.0 / n) : 0.0;
            size_t bar_len = static_cast<size_t>(std::round(buckets[i] * static_cast<double>(max_bar_width) / max_bucket));
            std::string bar;
            for(size_t j = 0; j < bar_len; ++j) bar += "█";

            std::format_to(
                std::back_inserter(log),
                "    [{:6.3f} - {:6.3f} ms] : {:>6} | {:<24} ({:5.1f}%)\n",
                b_start, b_end,
                buckets[i],
                bar,
                pct
            );
        }
        log.append("======================================================================\n");
    }
};

namespace {

auto make_profile(alib6::LogFactory& vklg) -> ave::ProfileWith {
    ave::ProfileWith with;
    with.configure_instance = [](ave::WithGlobalInput&, ave::CreateInstanceInfo& ci) {
        ci.application_name = "avetest";
        ci.api_version = ave::ave_vk_1_4;
    };
    with.configure_debug_messenger.emplace();
    with.try_dynamic_rendering = false;
    with.configure_debug_messenger->on_message = [&vklg](
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT,
        const VkDebugUtilsMessengerCallbackDataEXT& data
    ) {
        vklg(ave::to_log_level(severity))
            << (data.pMessage ? data.pMessage : "<no message>")
            << std::endl;
        return false;
    };
    return with;
}

inline void render_frame(ave::Renderer& renderer, const ave::Pipeline& pipeline) {
    auto graphics = renderer.acquire_context();
    graphics.begin();
    graphics.bind_pipeline(pipeline);
    graphics.draw(3);
    graphics.end();
}

void run_loop(
    ave::Window& window,
    ave::Renderer& renderer,
    const ave::Pipeline& pipeline,
    alib6::LogFactory& lg
) {
    FPSDetective detective;
    while (!window.should_close()) {
        window.poll_events();
        render_frame(renderer, pipeline);
        detective.next_frame();
    }
    lg << detective << std::endl;
}

} // namespace

auto main() -> int {
    try {
        alib6::Logger logger;
        alib6::LogFactory lg(logger, "avetest");
        alib6::LogFactory vklg(logger, "Vulkan");
        logger.append_mod<alib6::lot::Console>("console");

        ave::Context context;
        ave::Window window({
            .ctx = context,
            .title = "Hello from AVE!",
            .width = 800,
            .height = 600,
        });

        ave::RenderBuildReport report;
        auto renderer = ave::RenderProfile::from_window(context, window)
            .with(make_profile(vklg))
            .with_result(report)
            .build();

        lg << "Dynamic Rendering: "
           << report[ave::RenderBuildStageId::create_device].content["dynamic_rendering"].to<std::string_view>()
           << std::endl;

        auto pipeline = renderer.create_graphics_pipeline(
            "avetest/shaders/simple-vert.spv",
            "avetest/shaders/simple-frag.spv"
        );
        if (!pipeline) return 1;

        run_loop(window, renderer, *pipeline, lg);
    } catch (...) {
        // 已经有panic了，也是直接忽略
        return 1;
    }
    return 0;
}
