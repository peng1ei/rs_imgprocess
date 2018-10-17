//
// Created by penglei on 18-10-16.
//

#include "mg_striperemove.h"
#include <iostream>

namespace Mg {

    MgStripeRemove::MgStripeRemove(const std::string &fileIn,
            const std::string &fileOut,
            const std::string &format,
            StriperRemoveType method,
            int n)
        : fileIn_(fileIn), fileOut_(fileOut), format_(format),
        method_(method), n_(n), mgDatasetInPtr_(nullptr), mgDatasetOutPtr_(nullptr) {
        if (method_ == MgStripeRemove::MoveWindowWeight && n_%2 == 0)
            ++n_;
    }

    bool MgStripeRemove::run() {
        // 打开输入文件
        mgDatasetInPtr_ = makeMgDatasetManager();
        if (!mgDatasetInPtr_->openDataset(fileIn_.c_str())) {
            return false;
        }

        int bandCount = mgDatasetInPtr_->getRasterCount();
        int xSize = mgDatasetInPtr_->getRasterXSize();
        int ySize = mgDatasetInPtr_->getRasterYSize();
        auto gdt = mgDatasetInPtr_->getGdalDataType();

        // 创建输出文件
        mgDatasetOutPtr_ = makeMgDatasetManager();
        if (!mgDatasetOutPtr_->createDataset(fileOut_.c_str(), format_.c_str(),
                xSize, ySize, bandCount, gdt, nullptr)) {
            return false;
        }
        mgDatasetOutPtr_->setGeoTransform(mgDatasetInPtr_->getGeoTransform());
        mgDatasetOutPtr_->setProjection(mgDatasetInPtr_->getProjectionRef());

        if (method_ == MgStripeRemove::PloyFit) {
            MgSwitchGDALTypeProcess(gdt, ployFit);
        } else {
            MgSwitchGDALTypeProcess(gdt, moveWindowWeight);
        }
    }

    template <typename OutScalar>
    bool MgStripeRemove::ployFit() {
        // 用于计算多项式拟合系数，系数=(U'*U)-1 * U' * Y
        int urows = mgDatasetInPtr_->getRasterXSize();
        int ucols = n_ + 1;
        Mat::Matrixd U(urows, ucols);
        for (int j = 1; j <= urows; ++j) {
            for (int i = 0; i < ucols; ++i) {
                U(j-1, i) = std::pow(j, i);
            }
        }
        Mat::Matrixd &&matU = (U.transpose()*U).inverse()*U.transpose();

        int bandCount = mgDatasetInPtr_->getRasterCount();
        int height = mgDatasetInPtr_->getRasterYSize();
        int width = mgDatasetInPtr_->getRasterXSize();
        for (int i = 0; i < bandCount; ++i) {
            // 获取原始波段数据
            MgCube cube;
            if (!mgDatasetInPtr_->readDataChunk(false, 0, MgBandMap({i+1}), cube)) {
                return false;
            }

            // 计算原始列均值和列标准差
            Mat::Matrixd srcColMean = cube.band(0).colwise().mean().transpose();

            Mat::Matrixd srcColStdDev(srcColMean.rows(), 1);
            for (int col = 0; col < width; ++ col) {        // 按列处理
                auto &&vec = cube.band(0).col(col);

                for (int row = 0; row < height; ++ row) {
                    vec(row) -= srcColMean(col);            // 将原始数据的每一列减去原始列均值
                }
                srcColStdDev(col) = std::sqrt(vec.cwiseProduct(vec).sum()/height);
            }

            // 计算多项式拟合后的列均值和列标准差
            Mat::Matrixd &&newColMean = U*(matU*srcColMean);
            Mat::Matrixd &&newColStdDev = U*(matU*srcColStdDev);

            // 基于多项式拟合滤波的改进矩匹配法
            MgCube cubeOut(cube.height(), cube.width(), 1);
            auto &&coeff = newColStdDev.cwiseQuotient(srcColStdDev);

            if (std::is_floating_point<OutScalar>::value) {
                for (int col = 0; col < urows; ++ col) {        // 按列处理
                    auto &&vec = cube.band(0).col(col);
                    auto &&vecOut = cubeOut.band(0).col(col);

                    for (int row = 0; row < height; ++ row) {
                        // 改进的矩匹配法
                        cubeOut.band(i).col(col)(row) = (coeff(col)) * (vec(row)/* - srcColMean(col)*/) + newColMean(col);
                    }
                }
            } else {
                double tmpValue = 0;
                for (int col = 0; col < urows; ++ col) {        // 按列处理
                    auto &&vec = cube.band(0).col(col);
                    auto &&vecOut = cubeOut.band(0).col(col);

                    for (int row = 0; row < height; ++ row) {
                        // 改进的矩匹配法
                        tmpValue = (coeff(col)) * (vec(row)/* - srcColMean(col)*/) + newColMean(col);
                        tmpValue < 0 ? cubeOut.band(i).col(col)(row) = 0 :
                                cubeOut.band(i).col(col)(row) = tmpValue+0.5;
                    }
                }
            }

            if (!mgDatasetOutPtr_->writeDataChunk<OutScalar>(false, 0, MgBandMap({i+1}), cubeOut)) {
                return false;
            }
        }

        return true;
    }

    template <typename OutScalar>
    bool MgStripeRemove::moveWindowWeight() {
        int bandCount = mgDatasetInPtr_->getRasterCount();
        int height = mgDatasetInPtr_->getRasterYSize();
        int width = mgDatasetInPtr_->getRasterXSize();

        // 准备移动窗口
        Mat::Matrixd winWeight(1, n_);
        int halfWinSize = (n_ - 1) / 2;
        winWeight(halfWinSize) = halfWinSize+1;
        for (int i = 0; i < halfWinSize; ++i) {
            winWeight(i) = i+1;
            winWeight(n_ - i - 1) = winWeight(i);
        }

        double winWeightSum = winWeight.sum();

        for (int i = 0; i < bandCount; ++i) {
            // 获取原始波段数据
            MgCube cube;
            if (!mgDatasetInPtr_->readDataChunk(false, 0, MgBandMap({i+1}), cube)) {
                return false;
            }

            // 计算原始列均值和列标准差
            Mat::Matrixd srcColMean = cube.band(0).colwise().mean();

            Mat::Matrixd srcColStdDev(1, srcColMean.cols());
            for (int col = 0; col < width; ++ col) {    // 按列处理
                auto &&vec = cube.band(0).col(col);

                for (int row = 0; row < height; ++ row) {
                    vec(row) -= srcColMean(col);        // 将原始数据的每一列减去原始列均值
                }
                srcColStdDev(col) = std::sqrt(vec.cwiseProduct(vec).sum()/height);
            }

            // 基于移动窗口的方法，计算滤波后的列均值和列方差
            Mat::Matrixd newColMean(1, srcColMean.cols());
            Mat::Matrixd newColStdDev(1, srcColStdDev.cols());

            // 前 halfWinSize 和后 halfWinSize 的列均值和列标准差取原始的平均值
            {
                double leftMean = srcColMean.leftCols(halfWinSize).mean();
                double rightMean = srcColMean.rightCols(halfWinSize).mean();
                double leftStdDev = srcColStdDev.leftCols(halfWinSize).mean();
                double rightStdDev = srcColStdDev.rightCols(halfWinSize).mean();

                for (int h = 0; h < halfWinSize; ++h) {
                    newColMean(h) = leftMean;
                    newColMean(width - h - 1) = rightMean;

                    newColStdDev(h) = leftStdDev;
                    newColStdDev(width - h - 1) = rightStdDev;
                }
            }

            // 对中间的剩余列部分进行移动窗口(加权)滤波
            int endCols = width - halfWinSize;
            for (int j = halfWinSize; j < endCols; ++j) {
                auto &&meanBlock = srcColMean.block(0, j - halfWinSize, 1, n_);
                newColMean(j) = meanBlock.cwiseProduct(winWeight).sum() / winWeightSum;

                auto &&stdDevBlock = srcColStdDev.block(0, j - halfWinSize, 1, n_);
                newColStdDev(j) = stdDevBlock.cwiseProduct(winWeight).sum() / winWeightSum;
            }

            // 基于移动窗口(加权)滤波的改进矩匹配法
            MgCube cubeOut(cube.height(), cube.width(), 1);
            auto &&coeff = newColStdDev.cwiseQuotient(srcColStdDev);

            if (std::is_floating_point<OutScalar>::value) {
                for (int col = 0; col < width; ++ col) { // 列
                    auto &&vec = cube.band(0).col(col);
                    auto &&vecOut = cubeOut.band(0).col(col);
                    for (int row = 0; row < height; ++ row) {
                        cubeOut.band(i).col(col)(row) =
                                (coeff(col)) * (vec(row)/* - srcColMean(col)*/) + newColMean(col);
                    }
                }
            } else {
                double tmpValue = 0;
                for (int col = 0; col < width; ++ col) { // 列
                    auto &&vec = cube.band(0).col(col);
                    auto &&vecOut = cubeOut.band(0).col(col);

                    for (int row = 0; row < height; ++ row) {
                        tmpValue = (coeff(col)) * (vec(row)/* - srcColMean(col)*/) + newColMean(col);
                        tmpValue < 0 ? cubeOut.band(i).col(col)(row) = 0 :
                                cubeOut.band(i).col(col)(row) = tmpValue+0.5;
                    }
                }
            }

            if (!mgDatasetOutPtr_->writeDataChunk<OutScalar>(false, 0, MgBandMap({i+1}), cubeOut)) {
                return false;
            }
        }

        return true;
    }

} // namespace Mg
