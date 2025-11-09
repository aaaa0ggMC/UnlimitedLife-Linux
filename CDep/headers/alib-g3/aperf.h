/**
 * @file aperf.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 一个简单的性能计算库，能确保数据大致准确，同时省的我每次都要写差不多的benchmark代码
 * @version 0.1
 * @date 2025/11/09
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/11/08 
 */
#ifndef APERF_H_INCLUDED
#define APERF_H_INCLUDED
#include <alib-g3/aclock.h>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace alib::g3{
    /// @brief 进行热身的每轮次数
    constexpr uint32_t warm_up_per_run_times = 1'0000;
    /// @brief 最低热身时长
    constexpr uint32_t warm_up_time_ms = 100;
    
    template<class T> concept IsValidBenchmarkFunction = requires(T & t){
        t();
    };

    /// 单次运行结果
    struct SingleBenchmarkResult{
        double timeSum; ///< 单次运行总时间
        uint32_t times; ///< 运行总次数

        // 由于benchmark只会出现在debug下，不为性能考虑
        /// 输出为string
        inline std::string str() const{
            std::stringstream ss;
            ss << std::setw(9) << "TimeCost" << ":" << timeSum << "ms\n"
               << std::setw(9) << "RunTimes" << ":" << times << "\n"
               << std::setw(9) << "Average"  << ":" << timeSum/times;
            return ss.str();
        }
    };

    /// Repeat结果
    struct BenchmarkResults{
        std::string m_name;
        /// 结果列表
        std::vector<SingleBenchmarkResult> results;

        // 由于benchmark只会出现在debug下，不为性能考虑
        /// 输出为string
        inline std::string str() const{
            if(results.empty())return "";
            double sum = 0;
            double shortest_avg = __DBL_MAX__,longest_avg = 0;
            uint32_t times = 0;
            double stddev = 0;

            for(auto & t : results){
                sum += t.timeSum;
                times += t.times;
            }

            if(!times)return "";

            float global_aver = sum / times;
            for(auto & t : results){
                double thisAver = t.timeSum / t.times;

                if(thisAver > longest_avg)longest_avg = thisAver;
                else if(thisAver < shortest_avg)shortest_avg = thisAver;

                stddev += (thisAver - global_aver)*(thisAver-global_aver) * t.times;
            }
            stddev = std::sqrt(stddev / (times - 1));

            double cv = stddev/global_aver;

            static auto get_time = [](double t){
                constexpr uint32_t mover_size = 4;
                constexpr const char * mover[] = {"s","ms","us","ns"}; 
                std::string ret = "";
                int mindex = 1;
                while(0 < mindex && mindex < (mover_size - 1)){
                    if(t < 1){
                        mindex += 1;
                        t *= 1000;
                    }else if(t >= 1000){
                        mindex -= 1;
                        t /= 1000;
                    }else break;
                }
                ret += std::to_string(t);
                ret += mover[mindex];
                return ret;
            };

            std::stringstream ss;
            ss << "\n";
            ss << m_name << "-----------------------" << "\n";
            ss << std::setw(16) << "TimeCost" << ":" << get_time(sum) << "\n"
               << std::setw(16) << "RunTimes" << ":" << times << "\n"
               << std::setw(16) << "Average"  << ":" << get_time(global_aver) << "\n"
               << std::setw(16) << "ShortestAvgCall" << ":" << get_time(shortest_avg) << "\n"
               << std::setw(16) << "LongestAvgCall"  << ":" << get_time(longest_avg) << "\n"
               << std::setw(16) << "Stddev"  << ":" << stddev << "\n"
               << std::setw(16) << "CV"  << ":" << cv << "%";
            if(cv > 5.0)ss << "\nHigh variance!This benchmark may be invalid!";
            ss << "\n--------------------------------";
            return ss.str();
        }

        BenchmarkResults& name(std::string name){
            m_name = name;
            return *this;
        }
    };

    // 添加对alogger的流式输出
    namespace ext_toString{
        template<> inline auto toString(const SingleBenchmarkResult & r){
            return r.str();
        }

        template<> inline auto toString(const BenchmarkResults & r){
            return r.str();
        }
    }
    // 对ostream的流式输出
    std::ostream& operator<<(std::ostream & os,const SingleBenchmarkResult & val){
        os << val.str();
        return os;
    }
        std::ostream& operator<<(std::ostream & os,const BenchmarkResults val){
        os << val.str();
        return os;
    }

    /// 简易的benchmark函数
    template<IsValidBenchmarkFunction Function> struct Benchmark{
        Function func;

        // 初始化benchmark函数
        Benchmark(Function f):func(std::move(f)){}

        #pragma GCC push_options
        #pragma GCC optimize("O0")
        /**
         * @brief 运行benchmark
         * 
         * @param times 单次运行时长
         * @param repeat 重复次数
         * @return BenchmarkResults 
         * @start-date 2025/11/08
         */
        inline BenchmarkResults run(uint32_t times,uint32_t repeat){
            BenchmarkResults rs;

            /// 预热，同时计算出对volatile的修改
            {
                Clock clk;
                clk.start();
                uint64_t warmTimes = 0;
                uint64_t sink = 0;
                while(clk.getAllTime() < warm_up_time_ms){
                    for(int i = 0; i < warm_up_per_run_times; ++i)sink += 1;
                    warmTimes += warm_up_per_run_times;
                    sink = 0;
                }
            }

            for(uint32_t i = 0;i < repeat;++i){
                rs.results.push_back(single_run(times));
            }

            return rs;
        }
        #pragma GCC pop_options
    private:
        #pragma GCC push_options
        #pragma GCC optimize("O0")
        /// 单次运行测试
        inline SingleBenchmarkResult single_run(uint32_t times){
            Clock clk(false);
            SingleBenchmarkResult result = {0};

            clk.start();
            for(uint32_t i = 0;i < times;++i){
                func();
            }
            result.timeSum = clk.getAllTime();
            result.times = times;
            return result;
        }
        #pragma GCC pop_options
    };
}



#endif