//
// Created by penglei on 18-10-16.
//

#include "mg_striperemove.h"
#include <iostream>
#include <fstream>

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
        } else if (method_ == MgStripeRemove::MoveWindowWeight) {
            MgSwitchGDALTypeProcess(gdt, moveWindowWeight);
        } else if (method_ == MgStripeRemove::SecondaryGamma) {
            MgSwitchGDALTypeProcess(gdt, secondaryGamma);
        }
    }

    template <typename OutScalar>
    bool MgStripeRemove::ployFit() {
        // 用于计算多项式拟合系数，系数=(U'*U)-1 * U' * Y
        int urows = mgDatasetInPtr_->getRasterXSize();
        int ucols = n_ + 1;
        Mat::Matrixf U(urows, ucols);
        for (int j = 1; j <= urows; ++j) {
            for (int i = 0; i < ucols; ++i) {
                U(j-1, i) = std::pow(j, i);
            }
        }
        Mat::Matrixf &&matU = (U.transpose()*U).inverse()*U.transpose();

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
            Mat::Matrixf srcColMean = cube.band(0).colwise().mean().transpose();

            Mat::Matrixf srcColStdDev(srcColMean.rows(), 1);
            for (int col = 0; col < width; ++ col) {        // 按列处理
                auto &&vec = cube.band(0).col(col);

                for (int row = 0; row < height; ++ row) {
                    vec(row) -= srcColMean(col);            // 将原始数据的每一列减去原始列均值
                }
                srcColStdDev(col) = std::sqrt(vec.cwiseProduct(vec).sum()/height);
            }

            // 计算多项式拟合后的列均值和列标准差
            Mat::Matrixf &&newColMean = U*(matU*srcColMean);
            Mat::Matrixf &&newColStdDev = U*(matU*srcColStdDev);

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

        int heightOut = mgDatasetOutPtr_->getRasterYSize();
        int widthOut = mgDatasetOutPtr_->getRasterXSize();

        // 准备移动窗口
        Mat::Matrixf winWeight(1, n_);
        int halfWinSize = (n_ - 1) / 2;
        winWeight(halfWinSize) =  n_;//n_, halfWinSize+1;
        for (int i = 0; i < halfWinSize; ++i) {
            winWeight(i) = i+1;
            winWeight(n_ - i - 1) = winWeight(i);
        }
        double winWeightSum = winWeight.sum();

        // 针对欧比特数据，由于每一轨道的第0～1列和从第2530列开始直到末尾的那一列数据均不正常，因此需要剔除再计算
        int start = 0;     // 2需要处理的列起始位置（最小为0）
        int end = 2529;    // 2529需要处理的列结束位置（最大为width-1） clip2-1900
        int proWidth = end - start + 1; // 实际需要处理的列数

        for (int i = 0; i < bandCount; ++i) {
            MgCube cube;
            if (!mgDatasetInPtr_->readDataChunk(false, 0, MgBandMap({i+1}), cube)) {
                return false;
            }

            Mat::Matrixf srcColMeanExt(1, proWidth+2*halfWinSize);
            Mat::Matrixf srcColStdDevExt(1, proWidth+2*halfWinSize);

            {
                // 计算原始列均值和列标准差
                Mat::Matrixf srcColMean = (cube.band(0).colwise().mean()).block(0, start, 1, proWidth);
                Mat::Matrixf srcColStdDev(1, srcColMean.cols());

                for (int col = start, k = 0; col <= end; ++col, ++k) {    // 按列处理
                    auto &&vec = cube.band(0).col(col);

                    for (int row = 0; row < height; ++row) {
                        vec(row) -= srcColMean(k);                        // 将原始数据的每一列减去原始列均值
                    }
                    srcColStdDev(k) = std::sqrt(vec.cwiseProduct(vec).sum()/height);
                }

                // 进行拓展
                for (int i = 0; i < halfWinSize; ++i) {
                    // 向左拓展
                    srcColMeanExt(i) = srcColMean(0);
                    // 向右拓展
                    srcColMeanExt(proWidth+halfWinSize+i) = srcColMean(proWidth-1);

                    srcColStdDevExt(i) = srcColStdDev(0);
                    srcColStdDevExt(proWidth+halfWinSize+i) = srcColStdDev(proWidth-1);
                }

                srcColMeanExt.block(0,halfWinSize, 1, proWidth) = srcColMean.block(0,0,1,proWidth);
                srcColStdDevExt.block(0,halfWinSize, 1, proWidth) = srcColStdDev.block(0,0,1,proWidth);
            }

            Mat::Matrixf newColMean(1, proWidth);
            Mat::Matrixf newColStdDev(1, proWidth);

            /* 方案一 前 halfWinSize 和后 halfWinSize 的列均值和列标准差取原始的平均值
            // 前 halfWinSize 和后 halfWinSize 的列均值和列标准差取原始的平均值
//            {
//                double leftMean = srcColMean.leftCols(halfWinSize).mean();
//                double rightMean = srcColMean.rightCols(halfWinSize).mean();
//                double leftStdDev = srcColStdDev.leftCols(halfWinSize).mean();
//                double rightStdDev = srcColStdDev.rightCols(halfWinSize).mean();
//
//                for (int h = 0; h < halfWinSize; ++h) {
//                    newColMean(h) = leftMean;
//                    newColMean(width - h - 1) = rightMean;
//
//                    newColStdDev(h) = leftStdDev;
//                    newColStdDev(width - h - 1) = rightStdDev;
//                }
//            }


            // 对中间的剩余列部分进行移动窗口(加权)滤波
//            int endCols = width - halfWinSize;
//            for (int j = halfWinSize; j < endCols; ++j) {
//                auto &&meanBlock = srcColMean.block(0, j - halfWinSize, 1, n_);
//                newColMean(j) = meanBlock.cwiseProduct(winWeight).sum() / winWeightSum;
//
//                auto &&stdDevBlock = srcColStdDev.block(0, j - halfWinSize, 1, n_);
//                newColStdDev(j) = stdDevBlock.cwiseProduct(winWeight).sum() / winWeightSum;
//            }
             */

            // 基于加权的移动窗口的方法，计算滤波后的列均值和列方差
            for (int j = 0; j < proWidth; ++j) {
                auto &&meanBlock = srcColMeanExt.block(0, j, 1, n_);
                newColMean(j) = meanBlock.cwiseProduct(winWeight).sum() / winWeightSum;

                auto &&stdDevBlock = srcColStdDevExt.block(0, j, 1, n_);
                newColStdDev(j) = stdDevBlock.cwiseProduct(winWeight).sum() / winWeightSum;
            }

            // 基于移动窗口(加权)滤波的改进矩匹配法
            MgCube cubeOut(heightOut, widthOut, 1);
            Mat::Matrixf &&tmpSrcColStdDev = srcColStdDevExt.block(0,halfWinSize, 1, proWidth);
            auto &&coeff = newColStdDev.cwiseQuotient(tmpSrcColStdDev);

            // 用于调试
            ///////////////////////////////////////////////////////////////////////////////
            // 输出到文件中

//            std::fstream fs("/media/penglei/plwork/工作/HTHT/数据/欧比特/temp/test.txt", std::fstream::out);
////
////            // TODO 通过光谱角找出最吻合的窗口尺寸
//            {
//                Mat::Matrixf tmp1 = srcColMeanExt.block(0,halfWinSize, 1, width);
//                double dot = newColMean.cwiseProduct(tmp1).sum();
//                double x1 = std::sqrt(newColMean.cwiseProduct(newColMean).sum());
//                double x2 = std::sqrt(tmp1.cwiseProduct(tmp1).sum());
//
//                double dis = std::acos(dot/(x1*x2));
//                std::cout << dis << std::endl;
//
//            }
//
//            Mat::Matrixf tmp1 = srcColMeanExt.block(0,halfWinSize, 1, width);
//            for (int i = 0; i < width; ++i) {
//                fs << i+1 << "," << tmp1(i) << "," << newColMean(i) << "\n";
//            }
//
//            fs.close();

            //////////////////////////////////////////////////////////////////


            double tmpValue = 0;
            Mat::Matrixf &&tmpSrcColMean = srcColMeanExt.block(0,halfWinSize, 1, proWidth);
            if (std::is_floating_point<OutScalar>::value) {
                for (int col = start, k = 0; col <= end; ++col, ++k) { // 列
                    auto &&vec = cube.band(0).col(col);
                    auto &&vecOut = cubeOut.band(0).col(col);

                    for (int row = 0; row < height; ++row) {
                        tmpValue = coeff(k) * vec(row) + newColMean(k);
                        tmpValue += ( (vec(row)+tmpSrcColMean(k))/tmpSrcColMean(k)
                                - tmpValue/newColMean(k) )*tmpSrcColMean(k);

                        cubeOut.band(0).col(col)(row) = tmpValue;
                    }
                }
            } else {
                for (int col = start, k = 0; col <= end; ++ col, ++k) { // 列
                    auto &&vec = cube.band(0).col(col);
                    auto &&vecOut = cubeOut.band(0).col(col); // TODO 有可能会剔除那些坏列，直接输出有效的数据，这里就应该从0开始

                    for (int row = 0; row < height; ++row) {
                        tmpValue = coeff(k) * vec(row) + newColMean(k);

                        // TODO 对于影像中存在大面积的水域，可能会在水域有明显的条纹，因为补偿过度
                        // 对均匀地物进行补偿
                        // 对于欧比特来说，适应于后面的波段
//                        tmpValue += ( (vec(row)+tmpSrcColMean(k))/tmpSrcColMean(k)
//                         - tmpValue/newColMean(k) )*tmpSrcColMean(k);

                        tmpValue < 0 ? cubeOut.band(0).col(col)(row) = 0 :
                                cubeOut.band(0).col(col)(row) = tmpValue+0.5;
                    }
                }

//            if (start > 0) // 左边界
//                cubeOut.band(0).block(0, 0, height, start) =
//                        cube.band(0).block(0, 0, height, start);
//
//            if (end < width-1) // 右边界
//                cubeOut.band(0).block(0, end+1, height, width-proWidth-2) =
//                        cube.band(0).block(0, end+1, height, width-proWidth-2);
            }

            // 如果左右边界存在问题，则按原始值输出
            if (start > 0) // 左边界
                cubeOut.band(0).block(0, 0, height, start) =
                        cube.band(0).block(0, 0, height, start);

            if (end < width-1) // 右边界
                cubeOut.band(0).block(0, end+1, height, width-proWidth-2) =
                        cube.band(0).block(0, end+1, height, width-proWidth-2);

            if (!mgDatasetOutPtr_->writeDataChunk<OutScalar>(false, 0, MgBandMap({i+1}), cubeOut)) {
                return false;
            }
        }

        return true;
    }

    template <typename OutScalar>
    bool MgStripeRemove::secondaryGamma() {
        int bandCount = mgDatasetInPtr_->getRasterCount();
        int height = mgDatasetInPtr_->getRasterYSize();
        int width = mgDatasetInPtr_->getRasterXSize();

        int heightOut = mgDatasetOutPtr_->getRasterYSize();
        int widthOut = mgDatasetOutPtr_->getRasterXSize();

        int start = 0;     // 2需要处理的列起始位置（最小为0）
        int end = 2527;    // 2529需要处理的列结束位置（最大为width-1）
        int proWidth = end - start + 1; // 实际需要处理的列数

        int hightValue = 2000;
        for (int i = 0; i < bandCount; ++i) {
            MgCube cube;
            if (!mgDatasetInPtr_->readDataChunk(false, 0, MgBandMap({i + 1}), cube)) {
                return false;
            }


            float meanAll = meanExceptHightValue(cube.band(0), hightValue);
            std::cout << meanAll << std::endl;

            Mat::Matrixf meanCols(1, proWidth);
            for (int c = start; c < proWidth; ++c) {
                meanCols(c) = meanExceptHightValue(cube.band(0).col(c), hightValue);
            }

            // 列均值
            //Mat::Matrixf meanCols = (cube.band(0).colwise().mean()).block(0, start, 1, proWidth);

            // 基于移动窗口(加权)滤波的改进矩匹配法
            MgCube cubeOut(heightOut, widthOut, 1);
            for (int col = start, k = 0; col <= end; ++ col, ++k) { // 列
                auto &&vec = cube.band(0).col(col);
                auto &&vecOut = cubeOut.band(0).col(col); // TODO 有可能会剔除那些坏列，直接输出有效的数据，这里就应该从0开始

                for (int row = 0; row < height; ++row) {
                    if (vec(row) < hightValue) {
                        cubeOut.band(0).col(col)(row) = vec(row) * (meanAll / meanCols(k));

                    } else {
                        cubeOut.band(0).col(col)(row) = vec(row);
                    }
                }
            }

            // 如果左右边界存在问题，则按原始值输出
            if (start > 0) // 左边界
                cubeOut.band(0).block(0, 0, height, start) =
                        cube.band(0).block(0, 0, height, start);

            if (end < width-1) // 右边界
                cubeOut.band(0).block(0, end+1, height, width-proWidth-2) =
                        cube.band(0).block(0, end+1, height, width-proWidth-2);

            // 输出文件
            if (!mgDatasetOutPtr_->writeDataChunk<OutScalar>(false, 0, MgBandMap({i+1}), cubeOut)) {
                return false;
            }
        }

        return true;
    }

    float MgStripeRemove::meanExceptHightValue(const Mg::Mat::Matrixf &data, int hightValue) {
        int rows = data.rows();
        int cols = data.cols();

        float sum = 0;
        float num = 0;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (data(r, c) < hightValue) {
                    sum += data(r, c);
                    num++;
                }
            }
        }

        return sum/num;
    }

    /*
    float MgStripeRemove::meanExceptHightValue(const Mg::Mat::ExtMatrixf &data, int hightValue) {
        int rows = data.rows();
        int cols = data.cols();

        float sum = 0;
        float num = 0;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (data(r, c) < hightValue) {
                    sum += data(r, c);
                    num++;
                }
            }
        }

        return sum/num;
    }*/

} // namespace Mg
