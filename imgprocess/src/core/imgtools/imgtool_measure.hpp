//
// Created by penglei on 18-9-11.
//
// 用于测试代码运行时间

#ifndef IMGPROCESS_IMGTOOL_MEASURE_HPP
#define IMGPROCESS_IMGTOOL_MEASURE_HPP

#include <chrono>
#include <functional>
#include <iostream>

namespace ImgTool {

    // 用于测试算法时间
    // 精度修改
    // milliseconds : 毫秒
    // microseconds : 微秒
    // nanoseconds : 纳秒
    template<typename TimeT = std::chrono::milliseconds>
    struct measure {

        template<typename F, typename ...Args>
        static typename TimeT::rep execution(F &&fn, Args&&... args) {
            auto start = std::chrono::system_clock::now();

            // Now call the function with all the parameters you need.
            std::bind(std::forward<F>(fn), std::forward<Args>(args)...)();

            auto duration = std::chrono::duration_cast<TimeT>
                    (std::chrono::system_clock::now() - start);

            return duration.count();
        }



    };

    template<typename TimeT = std::chrono::milliseconds>
    void printRunTime(typename TimeT::rep time) {
        std::cout << "[Time: " << time << " ms]\n";
    }

    // 使用
    /*
    struct functor
    {
        int state;
        functor(int state) : state(state) {}
        void operator()() const
        {
            std::cout << "In functor run for ";
        }
    };

    void func()
    {
        std::cout << "In function, run for " << std::endl;
    }

    int main()
    {
        // codes directly
        std::cout << measure<>::execution([&]() {
            // your code
        }) << " ms" << std::endl;

        // functor
        std::cout << measure<>::execution(functor(3)) << std::endl;

        // function
        std::cout << measure<>::execution(func);
    }
     */

} // namespace ImgTool

#endif //IMGPROCESS_IMGTOOL_MEASURE_HPP
