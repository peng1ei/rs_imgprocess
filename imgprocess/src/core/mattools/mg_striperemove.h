//
// Created by penglei on 18-10-16.
//

#ifndef IMGPROCESS_MG_STRIPEREMOVE_H
#define IMGPROCESS_MG_STRIPEREMOVE_H

#include "mg_datasetmanager.h"
#include "mg_progress.hpp"
#include <string>

namespace Mg {

    class MgStripeRemove {
    public:
        enum StriperRemoveType {
            PloyFit,            /* 多项式拟合滤波 */
            MoveWindowWeight    /* 移动窗口(加权) */
        };

    public:
        /**
         *
         * @param fileIn    输入文件
         * @param fileOut   输出文件
         * @param format    输出文件格式，如 ENVI、GTiff等
         * @param method    条带噪声去除方法
         * @param n         如果选择“多项式拟合”，表示拟合的最高次数，推荐默认为5；
         *                  如果选择“移动窗口(加权)”，则表示窗口大小（奇数），推荐默认为41；
         */
        explicit MgStripeRemove(const std::string &fileIn,
                                const std::string &fileOut,
                                const std::string &format,
                                StriperRemoveType method,
                                int n);

        bool run();
    private:
        // 多项式拟合滤波
        template <typename OutScalar>
        bool ployFit();

        // 移动窗口（加权）滤波
        template <typename OutScalar>
        bool moveWindowWeight();

    private:
        std::string fileIn_;
        std::string fileOut_;
        std::string format_;

        StriperRemoveType method_;
        int n_;

        MgDatasetManagerPtr mgDatasetInPtr_;
        MgDatasetManagerPtr mgDatasetOutPtr_;
    };

} // namespace Mg



#endif //IMGPROCESS_MG_STRIPEREMOVE_H
