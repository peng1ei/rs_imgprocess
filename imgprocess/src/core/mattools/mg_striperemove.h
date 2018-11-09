//
// Created by penglei on 18-10-16.
//
// 高光谱图像条带去除算法（列条带）
// 优点：适应性强，对于均匀地物和非均匀地物的高光谱影像均有良好的去除条带效果。
// 待改进：针对影像上有大片水体时，处理完后，水体部分效果貌似不佳。
// 参考文献：《高光谱图像条带噪声去除方法研究与应用》支晶晶

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
            MoveWindowWeight,   /* 移动窗口(加权) */
            SecondaryGamma      /* 二次灰度系数校正法 */
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
        // 基于多项式拟合滤波
        // 此方法可使变化较缓和变化剧烈的数据，都获得良好的平滑效果。
        // 较好地恢复和保持地物真实反射率空间分布情况，明显改善了矩匹配方法产生的“带状效应”。
        template <typename OutScalar>
        bool ployFit();

        // 基于加权的移动窗口法滤波
        // 此方法一般不会发生图像灰度分布不均匀时应用矩匹配方法产生的失真，
        // 进行条带消除后各列灰度分布更符合自然地物的辐射分布。
        // 既降低了细节损失，又能取得良好的去条带效果。
        template <typename OutScalar>
        bool moveWindowWeight();

        // 二次灰度系数校正法
        template <typename OutScalar>
        bool secondaryGamma();

        float meanExceptHightValue(const Mat::Matrixf &data, int hightValue);
        //float meanExceptHightValue(const Mat::ExtMatrixf &data, int hightValue);

    private:
        std::string fileIn_;
        std::string fileOut_;
        std::string format_;

        StriperRemoveType method_;
        int n_;

        //int startCol_;
        //int endCol_;

        MgDatasetManagerPtr mgDatasetInPtr_;
        MgDatasetManagerPtr mgDatasetOutPtr_;
    };

} // namespace Mg



#endif //IMGPROCESS_MG_STRIPEREMOVE_H
