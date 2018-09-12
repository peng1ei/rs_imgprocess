//
// Created by penglei on 18-9-11.
//

#ifndef IMGPROCESS_IMGTOOL_MPCOMPUTESTATISTICS_HPP
#define IMGPROCESS_IMGTOOL_MPCOMPUTESTATISTICS_HPP

#include "imgtool_mpsingmultmodel.hpp"
#include "imgtool_progress.hpp"

class GDALDataset;

namespace ImgTool {

    class MpComputeStatistics : public ProgressFunctor {
    public:
        MpComputeStatistics(GDALDataset *dataset, int blkSize = 128) {
            imgDataset_ = dataset;
            blkSize_ = blkSize;
            imgBandCount_ = dataset->GetRasterCount();

            // todo 根据当前机器 CPU 核数以及需要处理的数据量去设置
            threadCount_ = 4;
        }

        virtual ~MpComputeStatistics() {
            for (int i = 0; i < threadCount_; i++) {
                delete[](means_[i]);
                means_[i] = nullptr;

                delete[](stdDevs_[i]);
                stdDevs_[i] = nullptr;

                delete[](covariances_[i]);
                covariances_[i] = nullptr;
            }
        }

        // 传入的统计量需要初始化为 0
        template <typename T>
        bool run(double *mean, double *stdDev,
                 double *covariance, double *correlation = nullptr) {

            // step 1: 创建一个 “单-多” 模型对象
            // todo 线程数和缓冲区队列数量的控制
            // todo 根据实际硬件条件（CPU核数）及任务量去决定
            ImgTool::Mp::MpSingleMultiModel<T> mp(threadCount_, threadCount_,
                    imgDataset_, blkSize_);
            mp.setProgress(progress_, std::placeholders::_1);

            // setp 2: 设置每一个消费者线程核心处理函数
            // 可以设置各个线程独立的参数
            for (int i = 0; i < threadCount_; i++) {
                means_.push_back(new double[imgBandCount_]{});
                stdDevs_.push_back(new double[imgBandCount_]{});
                covariances_.push_back(new double[imgBandCount_*imgBandCount_]{});

                mp.addProcessBlockData(std::bind(&MpComputeStatistics::processDataCore<T>,
                        this,
                        std::placeholders::_1,
                        means_[i],
                        stdDevs_[i],
                        covariances_[i]));
            }

            // step 3: 启动各个处理线程，并同步等待处理结果
            // 阻塞再此，直至所有线程结束
            mp.run();

            // step 4: 等待所有子线程处理完，合并数据
            for (int i = 0; i < imgBandCount_; i++) {
                for (int t = 0; t < threadCount_; t++) {
                    mean[i] += means_[t][i];
                    stdDev[i] += stdDevs_[t][i];
                }

                for (int j = 0; j < imgBandCount_; j++) {
                    int index = i*imgBandCount_ + j;

                    if (j < i) {
                        covariance[index] = covariance[j*imgBandCount_ + i];
                        continue;
                    }

                    for (int t = 0; t < threadCount_; t++) {
                        covariance[index] += covariances_[t][index];
                    }
                }
            }

            // 平均值
            int size = imgDataset_->GetRasterXSize()*imgDataset_->GetRasterYSize();
            for (int i = 0; i < imgBandCount_; i++) {
                mean[i] /= size;
            }

            // 标准差
            for (int i = 0; i < imgBandCount_; i++) {
                stdDev[i] /= size;
                stdDev[i] = std::sqrt(stdDev[i] - mean[i]*mean[i]);
            }

            // 协方差矩阵和相关系数矩阵
            for (int i = 0; i < imgBandCount_; i++) {
                for (int j = 0; j < imgBandCount_; j++) {
                    int index = i*imgBandCount_ + j;
                    covariance[i] /= size;
                    covariance[i] -= mean[i]*mean[j];

                    // 相关系数矩阵
                    if (correlation != nullptr) {
                        if (i == j) {
                            correlation[index] = 1.0;
                            continue;
                        }

                        if (stdDev[i] == 0) { // 避免下面的分母为 0
                            correlation[index] = 0;
                            continue;
                        }

                        correlation[index] = covariance[index] / (stdDev[i] * stdDev[j]);
                    }
                }
            }

            return true;
        }

        template <typename T>
        void processDataCore(ImgTool::ImgBlockData<T> &data,
                double *mean, double *stdDev, double *covariance) {
            int size = data.spatial().xSize() * data.spatial().ySize();
            T *pBuf1, *pBuf2;
            T *buf = data.bufData();

            for (int i = 0; i < size; i++) {
                int index = i*imgBandCount_;
                pBuf1 = buf + index;

                for (int b = 0; b < imgBandCount_; b++) {
                    pBuf2 = buf + b;

                    // sum of X: x1 + x2 + x3 + ...
                    mean[b] += pBuf2[index];

                    // sum of X^2: x1^2 + x2^2 + x3^2 + ...
                    stdDev[b] += pBuf2[index]*pBuf2[index];

                    // sum of X*Y: x1*y1 + x2*y2 + x3*y3 + ...
                    double *pCovar = covariance + b*imgBandCount_;
                    for (int b1 = b; b1 < imgBandCount_; b1++) {
                        pCovar[b1] = pBuf1[b1]*pBuf2[index];
                    }
                }

            }

        }

    private:

    private:
        GDALDataset *imgDataset_;
        int imgBandCount_;
        int blkSize_;

        int threadCount_;

        std::vector<double *> means_;
        std::vector<double *> stdDevs_;
        std::vector<double *> covariances_;
    };

} // namespace ImgTool

#endif //IMGPROCESS_IMGTOOL_MPCOMPUTESTATISTICS_HPP
