//
// Created by penglei on 18-10-11.
//

#ifndef IMGPROCESS_MG_COMPUTESTATISTICS_H
#define IMGPROCESS_MG_COMPUTESTATISTICS_H

#include "mg_datasetmanager.h"
#include "mg_progress.hpp"
#include <string>

namespace Mg {

    class MgComputeStatistics : public Progress1arg {
    public:
        explicit MgComputeStatistics(const std::string &file);

        bool run();

        Mat::Matrixd& mean() { return vMean_; }
        const Mat::Matrixd& mean() const { return vMean_; }

        Mat::Matrixd& stdDev() { return vStdDev_; }
        const Mat::Matrixd& stdDev() const { return vStdDev_; }

        Mat::Matrixd& covariance() { return matCova_; }
        const Mat::Matrixd& covariance() const { return matCova_; }

        Mat::Matrixd& correlation() { return matCorr_; }
        const Mat::Matrixd& correlation() const { return matCorr_; }

    private:
        /**
         * 用于计算分配给多线程任务的分割点（适用于正三角形，如协方差矩阵
         * 计算中的协方差计算），为了平均每个线程的任务量。
         * @param begin     起始的切分点
         * @param meanNum   每个线程的任务量
         * @param n         线程的个数
         * @param bands     波段数
         * @param point     返回的切分点
         */
        void findPoint(int begin, double meanNum, int n, int bands, std::vector<int> &point);

    private:
        std::string file_;
        Mat::Matrixd vMean_;
        Mat::Matrixd vStdDev_;
        Mat::Matrixd matCova_;
        Mat::Matrixd matCorr_;
    };


} // namespace Mg

#endif //IMGPROCESS_MG_COMPUTESTATISTICS_H
