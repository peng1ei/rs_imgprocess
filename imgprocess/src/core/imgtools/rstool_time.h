//
// Created by penglei on 18-9-29.
//

#ifndef IMGPROCESS_RSTOOL_TIME_H
#define IMGPROCESS_RSTOOL_TIME_H

#include <chrono>
#include <type_traits>
#include <iostream>

namespace RSTool {

    namespace Time {

        using ns = std::chrono::nanoseconds;
        using us = std::chrono::microseconds;
        using ms = std::chrono::milliseconds;
        using s = std::chrono::seconds;
        using m = std::chrono::minutes;
        using h = std::chrono::hours;

        template <typename DurationType> class AlgTime;
        using AlgTimeNs = AlgTime<ns>;
        using AlgTimeUs = AlgTime<us>;
        using AlgTimeMs = AlgTime<ms>;
        using AlgTimeS = AlgTime<s>;
        using AlgTimeM = AlgTime<m>;
        using AlgTimeH = AlgTime<h>;

        /**
         * 用于计算代码运行的时间
         * @tparam DurationType 类似于时间单位，默认以“毫秒”为单位
         */
        template <typename DurationType = ms>
        class AlgTime {
        public:

            void start() {
                start_  = std::chrono::steady_clock::now();
            }

            typename DurationType::rep elapsed() const {
                 auto elapsed = std::chrono::duration_cast<DurationType>(
                         std::chrono::steady_clock::now() - start_);
                return elapsed.count();
            }

            void printElapsed() {
                auto time = elapsed();

                if (std::is_same<DurationType, ns>::value) {
                    std::cout << std::endl << "[AlgTime: " << time << " ns]" << std::endl;
                } else if (std::is_same<DurationType, us>::value) {
                    std::cout << std::endl << "[AlgTime: " << time << " us]" << std::endl;
                } else if (std::is_same<DurationType, ms>::value) {
                    std::cout << std::endl << "[AlgTime: " << time << " ms]" << std::endl;
                } else if (std::is_same<DurationType, s>::value) {
                    std::cout << std::endl << "[AlgTime: " << time << " s]" << std::endl;
                } else if (std::is_same<DurationType, m>::value) {
                    std::cout << std::endl << "[AlgTime: " << time << " m]" << std::endl;
                } else if (std::is_same<DurationType, h>::value) {
                    std::cout << std::endl << "[AlgTime: " << time << " h]" << std::endl;
                }
            }

        private:
            std::chrono::steady_clock::time_point start_;
        };

    } // namespace Time

} // namespace RSTool

#endif //IMGPROCESS_RSTOOL_TIME_H
