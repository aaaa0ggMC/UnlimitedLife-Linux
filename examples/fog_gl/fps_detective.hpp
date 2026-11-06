/**
 * @file fps_detective.hpp
 * @brief 帧率/帧时间统计与报告 —— 由 `include/AVE/misc/fps_detective.cppm`
 *        (`ave::misc::FPSDetective` / `ave::misc::FPSResult`) 移植的示例内自带版本。
 *
 * 移植说明：
 *  - 不 `import ave`（fog_gl 是 AGE/OpenGL 示例，与 AVE/Vulkan 无关）；
 *  - 统计口径（含预热帧忽略、R-7 分位数、无偏矩、双判据卡顿、直方图长尾桶）与
 *    原版逐行等价，因此 shadow（小测）与正式 60s 测量的数字口径一致；
 *  - 表格输出使用 alib6 同族的 alib5::log_table + unicode_rounded() 边框，
 *    与 AVE 版 `alib6::Table(TableConfig::unicode_rounded())` 视觉格式一致，
 *    行列/文案/颜色一一对应；
 *  - 依赖仅 alib5（alib6 的非模块版本，AGE 现用的同族库）+ std。
 */

#ifndef FOG_GL_FPS_DETECTIVE_HPP
#define FOG_GL_FPS_DETECTIVE_HPP

#include <alib5/aclock.h>
#include <alib5/alogger.h>
#include <alib5/autil.h>
#include <alib5/compact/manip_table.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <format>
#include <string>
#include <vector>

namespace fog_gl::misc {

    namespace detail_fps {
        /// 与 alib6::log::color 等价的取色器（alib5 位于 lot 命名空间）
        inline constexpr auto color_cyan_bold = alib5::lot::color(
            alib5::lot::Color::Cyan, alib5::lot::Color::None, alib5::lot::Style::Bold);
        inline constexpr auto color_green = alib5::lot::color(alib5::lot::Color::Green);
        inline constexpr auto color_yellow = alib5::lot::color(alib5::lot::Color::Yellow);
        inline constexpr auto color_red = alib5::lot::color(alib5::lot::Color::Red);
        inline constexpr auto color_magenta = alib5::lot::color(alib5::lot::Color::Magenta);
        inline constexpr auto color_none = alib5::lot::color(alib5::lot::Color::None);
    } // namespace detail_fps

    /**
     * @brief 帧率与帧时间统计分析结果结构体
     */
    struct FPSResult {
        // 1. 基础概览
        std::uint64_t total_frames{0};
        std::uint64_t sampled_frames{0};
        double total_time_ms{0.0};
        double avg_fps{0.0};
        double avg_frame_time_ms{0.0};
        double min_frame_time_ms{0.0};
        double max_frame_time_ms{0.0};

        // 2. 分位数与离散度 (基于加权线性插值 R-7/NIST 统计标准)
        double p25{0.0};
        double p50{0.0}; // Median
        double p75{0.0};
        double p90{0.0};
        double p95{0.0};
        double p99{0.0};
        double p99_9{0.0};
        double iqr{0.0}; // P75 - P25

        // 3. 极端低帧 (Low FPS)
        double low_1pct_fps{0.0};    // 基于最慢 1% 帧均值 (Average of worst 1%)
        double low_01pct_fps{0.0};   // 基于最慢 0.1% 帧均值 (Average of worst 0.1%)
        double p99_fps{0.0};         // 基于 P99 分位数倒数 (1000 / P99)
        double p99_9_fps{0.0};       // 基于 P99.9 分位数倒数 (1000 / P99.9)

        // 4. 统计矩与分布形状 (经小样本无偏校正)
        double variance{0.0};        // 无偏样本方差 s^2
        double stddev{0.0};          // 样本标准差 s
        double cv_percent{0.0};      // 变异系数 (s / mean * 100%)
        double skewness{0.0};        // Fisher-Pearson 样本偏度系数
        double kurtosis{0.0};        // 样本超额峰度 (Excess Kurtosis, 正态分布 = 0)

        // 5. 帧平滑度与卡顿检测 (双判据判定)
        double avg_jitter_ms{0.0};   // 相邻帧耗时差绝对值的均值
        double max_jitter_ms{0.0};   // 最大单次相邻帧时间抖动跳变
        std::uint64_t stutter_count{0}; // 满足双判据的卡顿帧数
        double smoothness_score{0.0};// 平滑度得分 (0% ~ 100%)

        // 6. 帧时间直方图 (等宽主区间 + 异常长尾桶防变形)
        static constexpr std::size_t bucket_count = 6;
        struct Bucket {
            double start_ms{0.0};
            double end_ms{0.0};
            std::size_t count{0};
            double percentage{0.0};
            bool is_overflow{false};
        };
        std::array<Bucket, bucket_count> buckets{};
        std::size_t max_bucket_count{1};

        /// @brief 将统计结果格式化写入字符串（与 AVE 版 write_to_log 同格式）
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

            constexpr std::size_t max_bar_width = 24;
            for (std::size_t i = 0; i < FPSResult::bucket_count; ++i) {
                const auto& b = buckets[i];
                if (b.start_ms == 0.0 && b.end_ms == 0.0 && b.count == 0) continue;

                std::size_t bar_len = static_cast<std::size_t>(std::round(b.count * static_cast<double>(max_bar_width) / max_bucket_count));
                std::string bar;
                for (std::size_t j = 0; j < bar_len; ++j) bar += "█";

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

        /// @brief 将统计结果生成与 AVE 版同格式的美化 alib5::log_table
        /// @note  alib5 的 log_table 通过回调填充单元格（alib6::Table 是 operator[]），
        ///        行列/文案/颜色与 AVE 版逐行对应。
        [[nodiscard]] alib5::log_table table(
            alib5::log_table::cfg cfg = alib5::log_table::unicode_rounded()
        ) const {
            using detail_fps::color_cyan_bold;
            using detail_fps::color_green;
            using detail_fps::color_yellow;
            using detail_fps::color_red;
            using detail_fps::color_magenta;

            // 回调按值捕获结果，保证 log_table 晚于本对象存在
            const FPSResult res = *this;

            alib5::log_table tbl(
                [res]
                (alib5::detail::Operator& op) {
                    if (res.sampled_frames == 0) {
                        op[0][0] << alib5::lot::color(alib5::lot::Color::Yellow) << "FPS Detective";
                        op[0][1] << "No valid frame samples recorded";
                        return;
                    }

                    // 表头
                    op[0][0] << color_cyan_bold << "Metric / Dimension";
                    op[0][1] << color_cyan_bold << "Value";
                    op[0][2] << color_cyan_bold << "Details / Reference";

                    // 基础数据
                    op[1][0] << color_green << "Frames & Duration";
                    op[1][1] << std::format("{} frames", res.total_frames);
                    op[1][2] << std::format("{:.2f} s ({:.1f} ms, {} sampled)", res.total_time_ms / 1000.0, res.total_time_ms, res.sampled_frames);

                    op[2][0] << color_green << "Average FPS";
                    op[2][1] << std::format("{:.2f} FPS", res.avg_fps);
                    op[2][2] << std::format("Mean FT: {:.3f} ms", res.avg_frame_time_ms);

                    op[3][0] << color_green << "Median (P50)";
                    op[3][1] << std::format("{:.2f} FPS", res.p50 > 0.0 ? (1000.0 / res.p50) : 0.0);
                    op[3][2] << std::format("P50 FT: {:.3f} ms", res.p50);

                    op[4][0] << color_yellow << "1% Low FPS";
                    op[4][1] << std::format("{:.2f} FPS", res.low_1pct_fps);
                    op[4][2] << std::format("P99 FT: {:.3f} ms ({:.1f} FPS)", res.p99, res.p99_fps);

                    op[5][0] << color_yellow << "0.1% Low FPS";
                    op[5][1] << std::format("{:.2f} FPS", res.low_01pct_fps);
                    op[5][2] << std::format("P99.9 FT: {:.3f} ms ({:.1f} FPS)", res.p99_9, res.p99_9_fps);

                    op[6][0] << "Min / Max FT";
                    op[6][1] << std::format("{:.3f} / {:.3f} ms", res.min_frame_time_ms, res.max_frame_time_ms);
                    op[6][2] << std::format("Spread: {:.3f} ms", res.max_frame_time_ms - res.min_frame_time_ms);

                    op[7][0] << "P25 / P75 / IQR";
                    op[7][1] << std::format("{:.2f} / {:.2f} ms", res.p25, res.p75);
                    op[7][2] << std::format("IQR: {:.3f} ms", res.iqr);

                    op[8][0] << "Stddev & Jitter";
                    op[8][1] << std::format("{:.3f} ms (CV {:.1f}%)", res.stddev, res.cv_percent);
                    op[8][2] << std::format("Avg diff: {:.3f} ms | Spike: {:.3f} ms", res.avg_jitter_ms, res.max_jitter_ms);

                    op[9][0] << "Skewness / Kurtosis";
                    op[9][1] << std::format("{:+.2f} / {:+.2f}", res.skewness, res.kurtosis);
                    op[9][2] << (res.skewness > 0.5 ? "Right-tailed / Stutter spikes" : (res.skewness < -0.5 ? "Left-tailed" : "Normal"));

                    op[10][0] << color_magenta << "Smoothness Score";
                    op[10][1] << std::format("{:.2f}%", res.smoothness_score);
                    op[10][2] << std::format("{} stutters ({:.2f}%)", res.stutter_count, res.sampled_frames > 0 ? (res.stutter_count * 100.0 / res.sampled_frames) : 0.0);

                    // 直方图展示
                    std::uint32_t cur_row = 11;
                    for (std::size_t i = 0; i < FPSResult::bucket_count; ++i) {
                        const auto& b = res.buckets[i];
                        if (b.start_ms == 0.0 && b.end_ms == 0.0 && b.count == 0) continue;

                        std::size_t bar_len = static_cast<std::size_t>(std::round(b.count * 18.0 / res.max_bucket_count));
                        std::string bar;
                        for (std::size_t j = 0; j < bar_len; ++j) bar += "█";

                        if (b.is_overflow) {
                            op[cur_row][0] << color_red << std::format("Tail [>{:.2f}ms]", b.start_ms);
                        } else {
                            op[cur_row][0] << std::format("Hist [{:.2f}-{:.2f}ms]", b.start_ms, b.end_ms);
                        }
                        op[cur_row][1] << std::format("{:>5} ({:5.1f}%)", b.count, b.percentage);
                        op[cur_row][2] << bar;
                        ++cur_row;
                    }
                },
                alib5::_default_string_calc,
                nullptr,
                cfg
            );
            tbl.config.col_align = alib5::ColAlign::Left;
            return tbl;
        }
    };

    /**
     * @brief 高精度帧率与性能分析侦探（alib5::Clock 版本）
     */
    struct FPSDetective {
        using Result = FPSResult;

        std::uint64_t frames{0};
        alib5::Clock clock{false};
        std::deque<double> timestamps;
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
            return clock.status() == alib5::Clock::Running;
        }

        void next_frame() {
            ++frames;
            timestamps.emplace_back(clock.get_all());
        }

        [[nodiscard]] FPSResult calc() const {
            FPSResult res{};
            res.total_frames = frames;

            if (timestamps.empty()) {
                return res;
            }

            std::vector<double> deltas;
            deltas.reserve(timestamps.size());

            if (ignore_warmup_frame) {
                if (timestamps.size() >= 2) {
                    for (std::size_t i = 1; i < timestamps.size(); ++i) {
                        deltas.push_back(timestamps[i] - timestamps[i - 1]);
                    }
                } else {
                    return res;
                }
            } else {
                deltas.push_back(timestamps.front());
                for (std::size_t i = 1; i < timestamps.size(); ++i) {
                    deltas.push_back(timestamps[i] - timestamps[i - 1]);
                }
            }

            res.sampled_frames = deltas.size();
            if (deltas.empty()) return res;

            // 1. 严格自洽的总时长与均值
            double sum_time = 0.0;
            for (auto d : deltas) sum_time += d;
            res.total_time_ms = sum_time;

            const double n = static_cast<double>(deltas.size());
            res.avg_fps = sum_time > 0.0 ? (n * 1000.0 / sum_time) : 0.0;
            res.avg_frame_time_ms = n > 0.0 ? (sum_time / n) : 0.0;

            // 2. 排序与分位数插值 (R-7 / NIST 线性加权插值)
            std::vector<double> sorted = deltas;
            std::sort(sorted.begin(), sorted.end());

            auto get_percentile = [&](double pct) -> double {
                if (sorted.empty()) return 0.0;
                if (sorted.size() == 1) return sorted.front();
                pct = std::clamp<double>(pct, 0.0, 1.0);
                double rank = pct * (n - 1.0);
                std::size_t low = static_cast<std::size_t>(rank);
                std::size_t high = std::min(low + 1, sorted.size() - 1);
                double weight = rank - static_cast<double>(low);
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
                std::size_t count = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(sorted.size() * pct)));
                count = std::min(count, sorted.size());
                double sum = 0.0;
                for (std::size_t i = sorted.size() - count; i < sorted.size(); ++i) {
                    sum += sorted[i];
                }
                double avg = sum / count;
                return avg > 0.0 ? (1000.0 / avg) : 0.0;
            };
            res.low_1pct_fps = calc_worst_avg(0.01);
            res.low_01pct_fps = calc_worst_avg(0.001);

            // 4. 统计矩 (Fisher-Pearson 无偏小样本校正)
            double sum_sq = 0.0;
            double sum_cube = 0.0;
            double sum_quad = 0.0;
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
                double s = res.stddev;
                double s3 = s * s * s;
                double s4 = s3 * s;

                // Fisher-Pearson 偏度无偏估计
                if (n >= 3.0) {
                    res.skewness = (n / ((n - 1.0) * (n - 2.0))) * (sum_cube / s3);
                } else {
                    res.skewness = (sum_cube / n) / s3;
                }

                // 样本超额峰度无偏估计
                if (n >= 4.0) {
                    double term1 = (n * (n + 1.0)) / ((n - 1.0) * (n - 2.0) * (n - 3.0)) * (sum_quad / s4);
                    double term2 = (3.0 * (n - 1.0) * (n - 1.0)) / ((n - 2.0) * (n - 3.0));
                    res.kurtosis = term1 - term2;
                } else {
                    res.kurtosis = (sum_quad / n) / s4 - 3.0;
                }
            }

            // 5. 相邻帧抖动与双判据卡顿
            double sum_jitter = 0.0;
            res.max_jitter_ms = 0.0;
            res.stutter_count = 0;
            for (std::size_t i = 1; i < deltas.size(); ++i) {
                double j = std::abs(deltas[i] - deltas[i - 1]);
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
            res.smoothness_score = n > 0.0 ? std::clamp<double>(100.0 - (res.stutter_count * 100.0 / n), 0.0, 100.0) : 100.0;

            // 6. 直方图 (等宽桶 + 溢出长尾桶)
            const double outlier_threshold = std::max(res.p99 * 1.5, res.p75 + 3.0 * res.iqr);
            const bool has_extreme_tail = (res.max_frame_time_ms > outlier_threshold) && (sorted.size() > 10);
            const double hist_upper = has_extreme_tail ? outlier_threshold : res.max_frame_time_ms;
            const std::size_t regular_buckets = has_extreme_tail ? (FPSResult::bucket_count - 1) : FPSResult::bucket_count;

            const double range = hist_upper - res.min_frame_time_ms;
            const double bucket_step = (range > 0.0 && regular_buckets > 0) ? (range / regular_buckets) : 1.0;

            std::array<std::size_t, FPSResult::bucket_count> counts{};
            for (auto dt : deltas) {
                if (has_extreme_tail && dt >= hist_upper) {
                    counts[FPSResult::bucket_count - 1]++;
                } else {
                    std::size_t b = (bucket_step > 0.0) ? static_cast<std::size_t>((dt - res.min_frame_time_ms) / bucket_step) : 0;
                    if (b >= regular_buckets) b = regular_buckets - 1;
                    counts[b]++;
                }
            }

            res.max_bucket_count = 1;
            for (std::size_t i = 0; i < regular_buckets; ++i) {
                res.buckets[i].start_ms = res.min_frame_time_ms + i * bucket_step;
                res.buckets[i].end_ms = res.min_frame_time_ms + (i + 1) * bucket_step;
                res.buckets[i].count = counts[i];
                res.buckets[i].percentage = n > 0.0 ? (counts[i] * 100.0 / n) : 0.0;
                res.buckets[i].is_overflow = false;
                if (counts[i] > res.max_bucket_count) res.max_bucket_count = counts[i];
            }

            if (has_extreme_tail) {
                std::size_t last = FPSResult::bucket_count - 1;
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

        [[nodiscard]] alib5::log_table table(
            alib5::log_table::cfg cfg = alib5::log_table::unicode_rounded()
        ) const {
            return calc().table(cfg);
        }
    };

} // namespace fog_gl::misc

#endif // FOG_GL_FPS_DETECTIVE_HPP
