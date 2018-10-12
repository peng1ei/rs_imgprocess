//
// Created by penglei on 18-10-12.
//

#ifndef IMGPROCESS_MG_PROGRESS_HPP
#define IMGPROCESS_MG_PROGRESS_HPP

#include <functional>
#include <stdio.h>
#include <cstring>

namespace Mg {

    struct Progress1arg {
        template <class Fn, class... Args>
        void BindProgress1arg(Fn &&fn, Args &&... args) {
            progress1arg_ = std::bind(std::forward<Fn>(fn),
                    std::forward<Args>(args)...);
        }

    protected:
        std::function<int(double)> progress1arg_;
    };

    struct Progress2arg {
        template <class Fn, class... Args>
        void BindProgress2arg(Fn &&fn, Args &&... args) {
            progress2arg_ = std::bind(std::forward<Fn>(fn),
                    std::forward<Args>(args)...);
        }

    protected:
        std::function<int(double, const char *)> progress2arg_;
    };

    struct Progress3arg {
        template <class Fn, class... Args>
        void BindProgress3arg(Fn &&fn, Args &&... args) {
            progress3arg_ = std::bind(std::forward<Fn>(fn),
                    std::forward<Args>(args)...);
        }

    protected:
        std::function<int(double, const char *, void*)> progress3arg_;
    };

    // 控制台进度条
    struct ProgressTerm {

        ProgressTerm() {
            for (int i = 0; i < 41; i++) {
                buf[i] = '.';
            }
        }

        /**
         * 控制台进度条，模拟Linux进度条
         * @param pos   当前进度，范围为[0, 1]
         * @param msg   名称
         * @param arg   参数
         * @return
         */
        int operator() (double pos,
                        const char *msg,
                        void *arg = nullptr) {
            int rate = pos*40;
            pos *= 100;

            if (rate <= 40) {
                memset((char*)buf, '#', rate+1);

                if (rate < 40) {
                    printf("%s: [%-41s] [%d]%% [%c]\r", msg, buf, (int)pos, arr[rate%4]);
                    fflush(stdout);
                } else {
                    printf("%s: [%-41s] [%d]%% [Done]\n", msg, buf, (int)pos);
                }
            }

            return 0;
        }

    private:
        char buf[42] = {0};
        const char arr[4] = {'-', '\\', '|', '/'}; // 注意：'\'字符的表示
    };

} // namespace Mg

#endif //IMGPROCESS_MG_PROGRESS_HPP
