//
// Created by penglei on 18-10-11.
//

#include <future>
#include "mg_computestatistics.h"

namespace Mg {

    MgComputeStatistics::MgComputeStatistics(const std::string &file)
        : file_(file) { }

    bool MgComputeStatistics::run() {
        MgDatasetManagerPtr mgDatasetPtr = makeMgDatasetManager();
        if (!mgDatasetPtr->openDataset(file_.c_str())) {
            return false;
        }

        int bandCount = mgDatasetPtr->getRasterCount();
        vMean_   = Mat::Matrixd::Zero(bandCount, 1);
        vStdDev_ = Mat::Matrixd::Zero(bandCount, 1);
        matCova_ = Mat::Matrixd::Zero(bandCount, bandCount);
        matCorr_ = Mat::Matrixd::Zero(bandCount, bandCount);

        int tasks = (bandCount+1)*bandCount/2;
        int threads = getOptimalNumThreads(tasks) - 1;

        std::vector<int> point;
        {
            std::vector<int> tmpPoint;
            findPoint(bandCount, tasks/threads, threads, bandCount, tmpPoint);

            point.push_back(0);
            for (auto &value : tmpPoint) {
                point.push_back(value);
            }
            point.push_back(bandCount);
        }

        MgCube cube;
        MgBandMap bandMap(bandCount);
        std::vector<std::future<void>> fut(threads);

        if (progress1arg_) progress1arg_(0);

        int blkNum = mgDatasetPtr->blkNum();
        for (int k = 0; k < blkNum; ++k) {
            //TODO 独立线程读取数据
            mgDatasetPtr->readDataChunk(true, k, bandMap, cube);

            // 异步执行
            for (int i = 0; i < threads; ++i) {
                int start = point[i];
                int end = point[i+1];
                fut[i] = std::async(std::launch::async, [this, &cube, start, end, bandCount]() {
                    for (int b1 = start; b1 < end; ++b1) {
                        for (int b2 = b1+1; b2 < bandCount; ++b2) {
                            matCova_(b1, b2) += cube.data().row(b1)*cube.data().row(b2).transpose();
                        }
                    }
                });
            }

            vMean_ += cube.data().rowwise().sum();
            vStdDev_ += cube.data().cwiseProduct(cube.data()).rowwise().sum();

            for (auto &res : fut) {
                res.get();
            }

            if (progress1arg_) progress1arg_(k*1.0 / blkNum);
        } // end blk

        int size = mgDatasetPtr->getRasterXSize()*mgDatasetPtr->getRasterYSize();

        // 均值
        vMean_ /= size;

        // 标准差
        vStdDev_ = (vStdDev_ / size - vMean_.cwiseProduct(vMean_)).cwiseSqrt();

        // 协方差矩阵
        matCova_ = matCova_ / size - vMean_*vMean_.transpose();

        auto &&stdDev = vStdDev_.cwiseProduct(vStdDev_);
        for (int i = 0; i < bandCount; ++i) {
            matCova_(i, i) = stdDev(i);
            for (int j = 0; j < bandCount; ++j) {
                matCova_(j, i) = matCova_(i, j);
            }
        }

        // 相关系数矩阵
        matCorr_ = matCova_.cwiseQuotient(vStdDev_*vStdDev_.transpose());

        if (progress1arg_) progress1arg_(1.0);
        return true;
    }

    void MgComputeStatistics::findPoint(int begin, double meanNum,
            int n, int bands, std::vector<int> &point) {
        double sum = 0;
        while (true) {
            if (point.size() == n-1) return;

            sum += begin--;
            if (meanNum - sum <= 0 ) {
                point.push_back(bands - begin);
                findPoint(begin, meanNum, n, bands, point);
            }
        }
    }

} // namespace Mg
