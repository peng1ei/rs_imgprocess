//
// Created by penglei on 18-9-11.
//
// 进度条相关

#ifndef IMGPROCESS_IMGTOOL_PROGRESS_HPP
#define IMGPROCESS_IMGTOOL_PROGRESS_HPP

#include <functional>
#include <stdio.h>

namespace ImgTool {

    struct ProgressFunctor {
        template <class Fn, class... Args>
        void setProgress(Fn &&fn, Args &&... args) {
            progress_ = std::bind(std::forward<Fn>(fn),
                    std::forward<Args>(args)...);
        }

    protected:
        std::function<int(double)> progress_;
    };

    // 控制台进度条
    struct ProgressTerm {

       ProgressTerm() {
            for (int i = 0; i < 41; i++) {
                buf[i] = '.';
            }
        }

        int operator() (double pos,
                const char *msg,
                void *arg = nullptr) {

            // Linux 版进度条
            int rate = (int)(pos);
            rate = pos*40 / 100;

            if (rate <= 40) {
                buf[rate] = '#';
                if (rate < 40) {
                    printf("%s: [%-41s] [%d]%% [%c]\r", msg, buf, (int)pos, arr[rate%4]);
                    fflush(stdout);
                } else {
                    printf("%s: [%-41s] [%d]%% [Done] ", msg, buf, (int)pos);
                }
            }

//           // 简单版本
//            int rate = (int)(pos);
//            if (pos < 100) {
//                printf("%s: [%d]%% [%c]\r", msg, rate, arr[rate%4]);
//                fflush(stdout);
//            } else {
//                printf("%s: [100]%% [Done] ", msg);
//            }

            return 0;
        }

    private:
        char buf[42] = {0};
        const char arr[4] = {'-', '\\', '|', '/'}; // 注意：'\'字符的表示
    };

}

#endif //IMGPROCESS_IMGTOOL_PROGRESS_HPP
