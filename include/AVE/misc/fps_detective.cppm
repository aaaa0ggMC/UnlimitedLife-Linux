/**
 * @file fps_detective.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 高精度帧率侦探与帧时间高阶统计分析器 (ave.misc:fps_detective)
 * @version 1.0
 * @date 2026-09-12
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <AVE/config.h>
#include <alib6/debug.h>
#include <cmath>

export module ave.misc:fps_detective;

import std;
import alib6;

export namespace ave::misc {

    /**
     * @brief 帧率与帧时间统计分析结果结构体
     */
    struct FPSResult {
        // 1. 基础概览
        alib6::u64 total_frames{0};
        alib6::u64 sampled_frames{0};
        alib6::f64 total_time_ms{0.0};
        alib6::f64 avg_fps{0.0};
        alib6::f64 avg_frame_time_ms{0.0};
        alib6::f64 min_frame_time_ms{0.0};
        alib6::f64 max_frame_time_ms{0.0};

        // 2. 分位数与离散度 (基于加权线性插值 R-7/NIST 统计标准)
        alib6::f64 p25{0.0};
        alib6::f64 p50{0.0}; // Median
        alib6::f64 p75{0.0};
        alib6::f64 p90{0.0};
        alib6::f64 p95{0.0};
        alib6::f64 p99{0.0};
        alib6::f64 p99_9{0.0};
        alib6::f64 iqr{0.0}; // P75 - P25

        // 3. 极端低帧 (Low FPS)
        alib6::f64 low_1pct_fps{0.0};    // 基于最慢 1% 帧均值 (Average of worst 1%)
        alib6::f64 low_01pct_fps{0.0};   // 基于最慢 0.1% 帧均值 (Average of worst 0.1%)
        alib6::f64 p99_fps{0.0};         // 基于 P99 分位数倒数 (1000 / P99)
        alib6::f64 p99_9_fps{0.0};       // 基于 P99.9 分位数倒数 (1000 / P99.9)

        // 4. 统计矩与分布形状 (经小样本无偏校正)
        alib6::f64 variance{0.0};        // 无偏样本方差 s^2
        alib6::f64 stddev{0.0};          // 样本标准差 s
        alib6::f64 cv_percent{0.0};      // 变异系数 (s / mean * 100%)
        alib6::f64 skewness{0.0};        // Fisher-Pearson 样本偏度系数
        alib6::f64 kurtosis{0.0};        // 样本超额峰度 (Excess Kurtosis, 正态分布 = 0)

        // 5. 帧平滑度与卡顿检测 (双判据判定)
        alib6::f64 avg_jitter_ms{0.0};   // 相邻帧耗时差绝对值的均值
        alib6::f64 max_jitter_ms{0.0};   // 最大单次相邻帧时间抖动跳变
        alib6::u64 stutter_count{0};     // 满足双判据的卡顿帧数
        alib6::f64 smoothness_score{0.0};// 平滑度得分 (0% ~ 100%)

        // 6. 帧时间直方图 (等宽主区间 + 异常长尾桶防变形)
        static constexpr size_t bucket_count = 6;
        struct Bucket {
            alib6::f64 start_ms{0.0};
            alib6::f64 end_ms{0.0};
            size_t count{0};
            alib6::f64 percentage{0.0};
            bool is_overflow{false};
        };
        std::array<Bucket, bucket_count> buckets{};
        size_t max_bucket_count{1};

        /// @brief 将统计结果格式化写入字符串
        void write_to_log(std::pmr::string& log) const {
            if (sampled_frames == 0) {
                log.append("No valid frame samples recorded.\n");
                return;
            }

            std::format_to(
                std::back_inserter(log),
                "\n"
                "====================== [ FPS Detective Report ] ======================\n"
                "  [ Overview ]\n"
                "    Total / Sampled  : {} frames ({} sampled)\n"
                "    Duration         : {:.2f} s ({:.2f} ms)\n"
                "    Average FPS      : {:.2f} FPS  (Mean FT: {:.3f} ms)\n"
                "    Median FPS       : {:.2f} FPS  (P50 FT : {:.3f} ms)\n"
                "    1% Low FPS       : {:.2f} FPS  (P99 FT: {:.3f} ms)\n"
                "    0.1% Low FPS     : {:.2f} FPS  (P99.9 FT: {:.3f} ms)\n"
                "\n"
                "  [ Quantiles & Latency Spread ]\n"
                "    Min FT / Max FT  : {:.3f} ms / {:.3f} ms\n"
                "    P25 / P50 / P75  : {:.3f} ms / {:.3f} ms / {:.3f} ms\n"
                "    P90 / P95 / P99  : {:.3f} ms / {:.3f} ms / {:.3f} ms\n"
                "    IQR (P75 - P25)  : {:.3f} ms (Interquartile Range)\n"
                "\n"
                "  [ Statistical Moments & Distribution Shape ]\n"
                "    Stddev / CV      : {:.3f} ms (CV: {:.2f}%)\n"
                "    Inter-frame Diff : avg {:.3f} ms | max spike {:.3f} ms\n"
                "    Skewness (偏度)  : {:+.2f} ({})\n"
                "    Excess Kurtosis  : {:+.2f} ({})\n"
                "\n"
                "  [ Pacing & Smoothness ]\n"
                "    Stutter Frames   : {} frames ({:.2f}%)\n"
                "    Smoothness Score : {:.2f}%\n"
                "\n"
                "  [ Frame Time Distribution Histogram ]\n",
                total_frames, sampled_frames,
                total_time_ms / 1000.0, total_time_ms,
                avg_fps, avg_frame_time_ms,
                p50 > 0.0 ? (1000.0 / p50) : 0.0, p50,
                low_1pct_fps, p99,
                low_01pct_fps, p99_9,
                min_frame_time_ms, max_frame_time_ms,
                p25, p50, p75,
                p90, p95, p99,
                iqr,
                stddev, cv_percent,
                avg_jitter_ms, max_jitter_ms,
                skewness, (skewness > 0.5 ? "Right-tailed / Stutter spikes" : (skewness < -0.5 ? "Left-tailed" : "Symmetric")),
                kurtosis, (kurtosis > 1.0 ? "Leptokurtic / Fat-tailed outliers" : (kurtosis < -1.0 ? "Platykurtic / Uniform" : "Mesokurtic / Normal")),
                stutter_count, (sampled_frames > 0 ? (stutter_count * 100.0 / sampled_frames) : 0.0),
                smoothness_score
            );

            constexpr size_t max_bar_width = 24;
            for (size_t i = 0; i < FPSResult::bucket_count; ++i) {
                const auto& b = buckets[i];
                if (b.start_ms == 0.0 && b.end_ms == 0.0 && b.count == 0) continue;

                size_t bar_len = static_cast<size_t>(std::round(b.count * static_cast<double>(max_bar_width) / max_bucket_count));
                std::string bar;
                for (size_t j = 0; j < bar_len; ++j) bar += "█";

                if (b.is_overflow) {
                    std::format_to(
                        std::back_inserter(log),
                        "    [> {:6.3f} ms (Tail)] : {:>6} | {:<24} ({:5.1f}%)\n",
                        b.start_ms,
                        b.count,
                        bar,
                        b.percentage
                    );
                } else {
                    std::format_to(
                        std::back_inserter(log),
                        "    [{:6.3f} - {:6.3f} ms] : {:>6} | {:<24} ({:5.1f}%)\n",
                        b.start_ms, b.end_ms,
                        b.count,
                        bar,
                        b.percentage
                    );
                }
            }
            log.append("======================================================================\n");
        }

        /// @brief 将统计结果生成美化 alib6::Table
        [[nodiscard]] alib6::Table table(alib6::TableConfig cfg = alib6::TableConfig::unicode_rounded()) const {
            alib6::Table tbl(cfg);
            tbl.config.col_align = alib6::ColAlign::Left;

            if (sampled_frames == 0) {
                tbl[0][0] << alib6::log::color(alib6::log::Color::Yellow) << "FPS Detective";
                tbl[0][1] << "No valid frame samples recorded";
                return tbl;
            }

            // 表头
            tbl[0][0] << alib6::log::color(alib6::log::Color::Cyan, alib6::log::Color::None, alib6::log::Style::Bold) << "Metric / Dimension";
            tbl[0][1] << alib6::log::color(alib6::log::Color::Cyan, alib6::log::Color::None, alib6::log::Style::Bold) << "Value";
            tbl[0][2] << alib6::log::color(alib6::log::Color::Cyan, alib6::log::Color::None, alib6::log::Style::Bold) << "Details / Reference";

            // 基础数据
            tbl[1][0] << alib6::log::color(alib6::log::Color::Green) << "Frames & Duration";
            tbl[1][1] << std::format("{} frames", total_frames);
            tbl[1][2] << std::format("{:.2f} s ({:.1f} ms, {} sampled)", total_time_ms / 1000.0, total_time_ms, sampled_frames);

            tbl[2][0] << alib6::log::color(alib6::log::Color::Green) << "Average FPS";
            tbl[2][1] << std::format("{:.2f} FPS", avg_fps);
            tbl[2][2] << std::format("Mean FT: {:.3f} ms", avg_frame_time_ms);

            tbl[3][0] << alib6::log::color(alib6::log::Color::Green) << "Median (P50)";
            tbl[3][1] << std::format("{:.2f} FPS", p50 > 0.0 ? (1000.0 / p50) : 0.0);
            tbl[3][2] << std::format("P50 FT: {:.3f} ms", p50);

            tbl[4][0] << alib6::log::color(alib6::log::Color::Yellow) << "1% Low FPS";
            tbl[4][1] << std::format("{:.2f} FPS", low_1pct_fps);
            tbl[4][2] << std::format("P99 FT: {:.3f} ms ({:.1f} FPS)", p99, p99_fps);

            tbl[5][0] << alib6::log::color(alib6::log::Color::Yellow) << "0.1% Low FPS";
            tbl[5][1] << std::format("{:.2f} FPS", low_01pct_fps);
            tbl[5][2] << std::format("P99.9 FT: {:.3f} ms ({:.1f} FPS)", p99_9, p99_9_fps);

            tbl[6][0] << "Min / Max FT";
            tbl[6][1] << std::format("{:.3f} / {:.3f} ms", min_frame_time_ms, max_frame_time_ms);
            tbl[6][2] << std::format("Spread: {:.3f} ms", max_frame_time_ms - min_frame_time_ms);

            tbl[7][0] << "P25 / P75 / IQR";
            tbl[7][1] << std::format("{:.2f} / {:.2f} ms", p25, p75);
            tbl[7][2] << std::format("IQR: {:.3f} ms", iqr);

            tbl[8][0] << "Stddev & Jitter";
            tbl[8][1] << std::format("{:.3f} ms (CV {:.1f}%)", stddev, cv_percent);
            tbl[8][2] << std::format("Avg diff: {:.3f} ms | Spike: {:.3f} ms", avg_jitter_ms, max_jitter_ms);

            tbl[9][0] << "Skewness / Kurtosis";
            tbl[9][1] << std::format("{:+.2f} / {:+.2f}", skewness, kurtosis);
            tbl[9][2] << (skewness > 0.5 ? "Right-tailed / Stutter spikes" : (skewness < -0.5 ? "Left-tailed" : "Normal"));

            tbl[10][0] << alib6::log::color(alib6::log::Color::Magenta) << "Smoothness Score";
            tbl[10][1] << std::format("{:.2f}%", smoothness_score);
            tbl[10][2] << std::format("{} stutters ({:.2f}%)", stutter_count, sampled_frames > 0 ? (stutter_count * 100.0 / sampled_frames) : 0.0);

            // 直方图展示
            alib6::u32 cur_row = 11;
            for (size_t i = 0; i < FPSResult::bucket_count; ++i) {
                const auto& b = buckets[i];
                if (b.start_ms == 0.0 && b.end_ms == 0.0 && b.count == 0) continue;

                size_t bar_len = static_cast<size_t>(std::round(b.count * 18.0 / max_bucket_count));
                std::string bar;
                for (size_t j = 0; j < bar_len; ++j) bar += "█";

                if (b.is_overflow) {
                    tbl[cur_row][0] << alib6::log::color(alib6::log::Color::Red) << std::format("Tail [>{:.2f}ms]", b.start_ms);
                } else {
                    tbl[cur_row][0] << std::format("Hist [{:.2f}-{:.2f}ms]", b.start_ms, b.end_ms);
                }
                tbl[cur_row][1] << std::format("{:>5} ({:5.1f}%)", b.count, b.percentage);
                tbl[cur_row][2] << bar;
                ++cur_row;
            }

            return tbl;
        }
    };

    /**
     * @brief 高精度帧率与性能分析侦探
     */
    struct AVE_API FPSDetective {
        using Result = FPSResult;

        alib6::u64 frames{0};
        alib6::Clock clock{false};
        std::deque<alib6::f64> timestamps;
        bool ignore_warmup_frame{true}; // 默认开启：忽略首帧冷启动干扰，将首帧作为时间原点锚点

        explicit FPSDetective(bool ignore_warmup = true)
            : ignore_warmup_frame(ignore_warmup) {}

        void start() {
            clock.start();
        }

        void pause() {
            clock.pause();
        }

        void resume() {
            clock.resume();
        }

        void reset() {
            clock.stop();
            frames = 0;
            timestamps.clear();
        }

        [[nodiscard]] bool is_running() const noexcept {
            return clock.status() == alib6::Clock::Status::Running;
        }

        void next_frame() {
            panic_debug(
                clock.status() != alib6::Clock::Status::Running,
                "ave::misc::FPSDetective::next_frame() called while clock is not running! Did you forget to call start()?"
            );
            ++frames;
            timestamps.emplace_back(clock.get_all());
        }

        [[nodiscard]] FPSResult calc() const {
            FPSResult res{};
            res.total_frames = frames;

            if (timestamps.empty()) {
                return res;
            }

            std::vector<alib6::f64> deltas;
            deltas.reserve(timestamps.size());

            if (ignore_warmup_frame) {
                if (timestamps.size() >= 2) {
                    for (size_t i = 1; i < timestamps.size(); ++i) {
                        deltas.push_back(timestamps[i] - timestamps[i - 1]);
                    }
                } else {
                    return res;
                }
            } else {
                deltas.push_back(timestamps.front());
                for (size_t i = 1; i < timestamps.size(); ++i) {
                    deltas.push_back(timestamps[i] - timestamps[i - 1]);
                }
            }

            res.sampled_frames = deltas.size();
            if (deltas.empty()) return res;

            // 1. 严格自洽的总时长与均值
            alib6::f64 sum_time = 0.0;
            for (auto d : deltas) sum_time += d;
            res.total_time_ms = sum_time;

            const alib6::f64 n = static_cast<alib6::f64>(deltas.size());
            res.avg_fps = sum_time > 0.0 ? (n * 1000.0 / sum_time) : 0.0;
            res.avg_frame_time_ms = n > 0.0 ? (sum_time / n) : 0.0;

            // 2. 排序与分位数插值 (R-7 / NIST 线性加权插值)
            std::vector<alib6::f64> sorted = deltas;
            std::sort(sorted.begin(), sorted.end());

            auto get_percentile = [&](alib6::f64 pct) -> alib6::f64 {
                if (sorted.empty()) return 0.0;
                if (sorted.size() == 1) return sorted.front();
                pct = std::clamp<alib6::f64>(pct, 0.0, 1.0);
                alib6::f64 rank = pct * (n - 1.0);
                size_t low = static_cast<size_t>(rank);
                size_t high = std::min(low + 1, sorted.size() - 1);
                alib6::f64 weight = rank - static_cast<alib6::f64>(low);
                return sorted[low] * (1.0 - weight) + sorted[high] * weight;
            };

            res.min_frame_time_ms = sorted.front();
            res.max_frame_time_ms = sorted.back();
            res.p25 = get_percentile(0.25);
            res.p50 = get_percentile(0.50);
            res.p75 = get_percentile(0.75);
            res.p90 = get_percentile(0.90);
            res.p95 = get_percentile(0.95);
            res.p99 = get_percentile(0.99);
            res.p99_9 = get_percentile(0.999);
            res.iqr = res.p75 - res.p25;

            // 3. 1% Low & 0.1% Low (分位数对应 FPS 与 最差均值)
            res.p99_fps = res.p99 > 0.0 ? (1000.0 / res.p99) : 0.0;
            res.p99_9_fps = res.p99_9 > 0.0 ? (1000.0 / res.p99_9) : 0.0;

            auto calc_worst_avg = [&](double pct) -> double {
                size_t count = std::max<size_t>(1, static_cast<size_t>(std::ceil(sorted.size() * pct)));
                count = std::min(count, sorted.size());
                double sum = 0.0;
                for (size_t i = sorted.size() - count; i < sorted.size(); ++i) {
                    sum += sorted[i];
                }
                double avg = sum / count;
                return avg > 0.0 ? (1000.0 / avg) : 0.0;
            };
            res.low_1pct_fps = calc_worst_avg(0.01);
            res.low_01pct_fps = calc_worst_avg(0.001);

            // 4. 统计矩 (Fisher-Pearson 无偏小样本校正)
            alib6::f64 sum_sq = 0.0;
            alib6::f64 sum_cube = 0.0;
            alib6::f64 sum_quad = 0.0;
            for (auto dt : deltas) {
                auto diff = dt - res.avg_frame_time_ms;
                auto d2 = diff * diff;
                sum_sq += d2;
                sum_cube += d2 * diff;
                sum_quad += d2 * d2;
            }

            res.variance = n > 1.0 ? (sum_sq / (n - 1.0)) : 0.0;
            res.stddev = std::sqrt(res.variance);
            res.cv_percent = res.avg_frame_time_ms > 0.0 ? (res.stddev / res.avg_frame_time_ms * 100.0) : 0.0;

            if (res.stddev > 1e-9) {
                alib6::f64 s = res.stddev;
                alib6::f64 s3 = s * s * s;
                alib6::f64 s4 = s3 * s;

                // Fisher-Pearson 偏度无偏估计
                if (n >= 3.0) {
                    res.skewness = (n / ((n - 1.0) * (n - 2.0))) * (sum_cube / s3);
                } else {
                    res.skewness = (sum_cube / n) / s3;
                }

                // 样本超额峰度无偏估计
                if (n >= 4.0) {
                    alib6::f64 term1 = (n * (n + 1.0)) / ((n - 1.0) * (n - 2.0) * (n - 3.0)) * (sum_quad / s4);
                    alib6::f64 term2 = (3.0 * (n - 1.0) * (n - 1.0)) / ((n - 2.0) * (n - 3.0));
                    res.kurtosis = term1 - term2;
                } else {
                    res.kurtosis = (sum_quad / n) / s4 - 3.0;
                }
            }

            // 5. 相邻帧抖动与双判据卡顿
            alib6::f64 sum_jitter = 0.0;
            res.max_jitter_ms = 0.0;
            res.stutter_count = 0;
            for (size_t i = 1; i < deltas.size(); ++i) {
                alib6::f64 j = std::abs(deltas[i] - deltas[i - 1]);
                sum_jitter += j;
                if (j > res.max_jitter_ms) res.max_jitter_ms = j;

                // 双判据卡顿判定：
                // 判据 1: 耗时超中位数 1.5 倍 且 相对前一帧抖动 > 4.0ms
                // 判据 2: 耗时超中位数 2.5 倍
                if ((deltas[i] > 1.5 * res.p50 && j > 4.0) || (deltas[i] > 2.5 * res.p50)) {
                    ++res.stutter_count;
                }
            }
            res.avg_jitter_ms = deltas.size() > 1 ? (sum_jitter / (deltas.size() - 1)) : 0.0;
            res.smoothness_score = n > 0.0 ? std::clamp<alib6::f64>(100.0 - (res.stutter_count * 100.0 / n), 0.0, 100.0) : 100.0;

            // 6. 直方图 (等宽桶 + 溢出长尾桶)
            const alib6::f64 outlier_threshold = std::max(res.p99 * 1.5, res.p75 + 3.0 * res.iqr);
            const bool has_extreme_tail = (res.max_frame_time_ms > outlier_threshold) && (sorted.size() > 10);
            const alib6::f64 hist_upper = has_extreme_tail ? outlier_threshold : res.max_frame_time_ms;
            const size_t regular_buckets = has_extreme_tail ? (FPSResult::bucket_count - 1) : FPSResult::bucket_count;

            const alib6::f64 range = hist_upper - res.min_frame_time_ms;
            const alib6::f64 bucket_step = (range > 0.0 && regular_buckets > 0) ? (range / regular_buckets) : 1.0;

            std::array<size_t, FPSResult::bucket_count> counts{};
            for (auto dt : deltas) {
                if (has_extreme_tail && dt >= hist_upper) {
                    counts[FPSResult::bucket_count - 1]++;
                } else {
                    size_t b = (bucket_step > 0.0) ? static_cast<size_t>((dt - res.min_frame_time_ms) / bucket_step) : 0;
                    if (b >= regular_buckets) b = regular_buckets - 1;
                    counts[b]++;
                }
            }

            res.max_bucket_count = 1;
            for (size_t i = 0; i < regular_buckets; ++i) {
                res.buckets[i].start_ms = res.min_frame_time_ms + i * bucket_step;
                res.buckets[i].end_ms = res.min_frame_time_ms + (i + 1) * bucket_step;
                res.buckets[i].count = counts[i];
                res.buckets[i].percentage = n > 0.0 ? (counts[i] * 100.0 / n) : 0.0;
                res.buckets[i].is_overflow = false;
                if (counts[i] > res.max_bucket_count) res.max_bucket_count = counts[i];
            }

            if (has_extreme_tail) {
                size_t last = FPSResult::bucket_count - 1;
                res.buckets[last].start_ms = hist_upper;
                res.buckets[last].end_ms = res.max_frame_time_ms;
                res.buckets[last].count = counts[last];
                res.buckets[last].percentage = n > 0.0 ? (counts[last] * 100.0 / n) : 0.0;
                res.buckets[last].is_overflow = true;
                if (counts[last] > res.max_bucket_count) res.max_bucket_count = counts[last];
            }

            return res;
        }

        void write_to_log(std::pmr::string& log) const {
            calc().write_to_log(log);
        }

        [[nodiscard]] alib6::Table table(alib6::TableConfig cfg = alib6::TableConfig::unicode_rounded()) const {
            return calc().table(cfg);
        }
    };

} // namespace ave::misc

export namespace std {
    template<>
    struct formatter<ave::misc::FPSResult> : formatter<string_view> {
        auto format(const ave::misc::FPSResult& res, format_context& ctx) const {
            pmr::string buf;
            res.write_to_log(buf);
            return formatter<string_view>::format(buf, ctx);
        }
    };
}
