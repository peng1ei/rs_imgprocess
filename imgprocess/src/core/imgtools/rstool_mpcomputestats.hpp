//
// Created by penglei on 18-9-21.
//

#ifndef IMGPROCESS_RSTOOL_MPCOMPUTESTATS_HPP
#define IMGPROCESS_RSTOOL_MPCOMPUTESTATS_HPP

#include "rstool_rpmodel.hpp"

namespace RSTool {

    class MpComputeStatistics {
    public:
        MpComputeStatistics(const std::string &infile, int blkSize = 128)
                : infile_(infile), blkSize_(blkSize), threadCount_(1),
                  imgDataset_(nullptr) {
        }

        virtual ~MpComputeStatistics() {
            for (auto &mean : means_) {
                ReleaseArray(mean);
            }

            for (auto &stdDev : stdDevs_) {
                ReleaseArray(stdDev);
            }

            for (auto &covariance : covariances_) {
                ReleaseArray(covariance);
            }

            GDALClose((GDALDatasetH)imgDataset_);
        }

        /**
         * 计算影像的基本统计信息
         * @param mean          均值
         * @param stdDev        标准差
         * @param covariance    协方差矩阵
         * @param correlation   相关系数矩阵
         * @return 成功返回 true，失败返回 false
         */
        bool run(double *mean,
                double *stdDev,
                double *covariance,
                double *correlation = nullptr) {

            GDALAllRegister();
            imgDataset_ = (GDALDataset*)GDALOpen(infile_.c_str(), GA_ReadOnly);
            if (imgDataset_ == nullptr) {
                return false;
            }

            imgBandCount_ = imgDataset_->GetRasterCount();
            GDALDataType dataType = imgDataset_->GetRasterBand(1)->GetRasterDataType();
            switch (dataType) {
                case GDALDataType::GDT_Byte:
                    exec<unsigned char>(mean, stdDev, covariance, correlation);
                    break;
                case GDALDataType::GDT_UInt16:
                    exec<unsigned short>(mean, stdDev, covariance, correlation);
                    break;
                case GDALDataType::GDT_Int16:
                    exec<short>(mean, stdDev, covariance, correlation);
                    break;
                case GDALDataType::GDT_UInt32:
                    exec<unsigned int>(mean, stdDev, covariance, correlation);
                    break;
                case GDALDataType::GDT_Int32:
                    exec<int>(mean, stdDev, covariance, correlation);
                    break;
                case GDALDataType::GDT_Float32:
                    exec<float>(mean, stdDev, covariance, correlation);
                    break;
                case GDALDataType::GDT_Float64:
                    exec<double>(mean, stdDev, covariance, correlation);
                    break;
                default:
                    return false;
            }

            return true;
        }

    private:
        template <typename T>
        void exec(double *mean,
                  double *stdDev,
                  double *covariance,
                  double *correlation) {
            // step 1: 创建一个 “rp-model” 对象
            Mp::MpRPModel<T> rp(infile_, SpectralDimes(imgBandCount_));
            threadCount_ = rp.consumerCount();

            // setp 2: 设置每一个消费者线程入口函数
            // 根据需要，可以设置各个线程独立的参数
            for (int i = 0; i < threadCount_; i++) {
                means_.push_back(new double[imgBandCount_]{});
                stdDevs_.push_back(new double[imgBandCount_]{});
                covariances_.push_back(new double[imgBandCount_*imgBandCount_]{});

                rp.emplaceTask(std::bind(&MpComputeStatistics::processDataCore<T>,
                        this,
                        std::placeholders::_1,
                        means_[i],
                        stdDevs_[i],
                        covariances_[i]));
            }

            // step 3: 启动各个处理线程，并同步等待处理结果
            // 阻塞再此，直至所有线程结束
            rp.run();

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
                    covariance[index] /= size;
                    covariance[index] -= mean[i]*mean[j];

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
        } // end exec()

        template <typename T>
        void processDataCore(DataChunk<T> &data,
                             double *mean,
                             double *stdDev,
                             double *covariance) {
            int size = data.dims().spatialSize();
            T *pBuf1, *pBuf2;
            T *buf = data.data();
            double *pCovar = nullptr;

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
                    pCovar = covariance + b*imgBandCount_;
                    for (int b1 = b; b1 < imgBandCount_; b1++) {
                        pCovar[b1] += pBuf1[b1]*pBuf2[index];
                    }
                } // end for b

            } // end for elem

        } // end processDataCore()
    private:
        std::string infile_;
        int blkSize_;
        int threadCount_;

        GDALDataset *imgDataset_;
        int imgBandCount_;

        std::vector<double *> means_;
        std::vector<double *> stdDevs_;
        std::vector<double *> covariances_;
    };

} // namespace RSTool

#endif //IMGPROCESS_RSTOOL_MPCOMPUTESTATS_HPP
