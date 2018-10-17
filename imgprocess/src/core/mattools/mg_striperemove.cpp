//
// Created by penglei on 18-10-16.
//

#include "mg_striperemove.h"
#include <iostream>

namespace Mg {

    MgStripeRemove::MgStripeRemove(const std::string &fileIn,
            const std::string &fileOut,
            const std::string &format, int n)
        : fileIn_(fileIn), fileOut_(fileOut), format_(format), n_(n) {

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

        MgSwitchGDALTypeProcess(gdt);
        return true;
    }

    template <typename OutScalar>
    bool MgStripeRemove::commonProcess() {

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
        int height = mgDatasetInPtr_->getRasterYSize();;
        int width = mgDatasetInPtr_->getRasterXSize();;
        for (int i = 0; i < bandCount; ++i) {

            // 获取原始数据
            MgCube cube;
            if (!mgDatasetInPtr_->readDataChunk(false, 0, MgBandMap({i+1}), cube)) {
                return false;
            }

            // 计算列均值和列标准差
            Mat::Matrixd srcColMean = cube.band(0).colwise().mean().transpose();
            Mat::Matrixd srcColStdDev(srcColMean.rows(), 1);

            for (int col = 0; col < width; ++ col) { // 列
                auto &&vec = cube.band(0).col(col);

                for (int row = 0; row < height; ++ row) {
                    vec(row) -= srcColMean(col); // 减均值
                }
                srcColStdDev(col) = std::sqrt(vec.cwiseProduct(vec).sum()/height);
            }

            // 计算多项式拟合后的列均值和列标准差
            Mat::Matrixd &&newColMean = U*(matU*srcColMean);
            Mat::Matrixd &&newColStdDev = U*(matU*srcColStdDev);

            // 基于多项式拟合滤波的改进矩匹配法
            MgCube cubeOut(cube.height(), cube.width(), 1);
            auto &&coeff = newColStdDev.cwiseQuotient(srcColStdDev);

            double tmpValue = 0;
            for (int col = 0; col < urows; ++ col) { // 列
                auto &&vec = cube.band(0).col(col);
                auto &&vecOut = cubeOut.band(0).col(col);
                for (int row = 0; row < height; ++ row) {
                    tmpValue = (coeff(col)) * (vec(row)/* - srcColMean(col)*/) + newColMean(col);
                    tmpValue < 0 ? cubeOut.band(i).col(col)(row) = 0 : cubeOut.band(i).col(col)(row) = tmpValue+0.5;
                }
            }

            if (!mgDatasetOutPtr_->writeDataChunk<OutScalar>(false, 0, MgBandMap({i+1}), cubeOut)) {
                return false;
            }
        }

        return true;
    }

} // namespace Mg
