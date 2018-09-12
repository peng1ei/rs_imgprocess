//
// Created by penglei on 18-9-12.
//

#include "ia_anomalydetection.h"
#include "imgtool_mpcomputestatistics.hpp"
#include "imgtool_mpsingmultmodel.hpp"
#include "mattool_common.h"
#include "gdal_priv.h"

namespace ImgAlgo {

    RXAnomalyDetection::RXAnomalyDetection(const std::string &inFile,
            const std::string &outFile, const std::string &outFormat,
            ImgAlgo::RXType rxtType/* = RXD */) {

        inFile_ = inFile;
        outFile_ = outFile;
        outFileFormat_ = outFormat;
        rxdType_ = rxtType;

        poInDS_ = nullptr;
        poOutDS_ = nullptr;
        pMean_ = nullptr;
        pCovariance_ = nullptr;
    }

    bool RXAnomalyDetection::init() {
        GDALAllRegister();

        GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName(outFileFormat_.c_str());
        if (poDriver == nullptr) {
            // TODO: 添加错误信息
            setErrorMsg(ERR_DRIVER_MSG);
            return false;
        }

        poInDS_ = (GDALDataset *)GDALOpen(inFile_.c_str(), GA_ReadOnly);
        if (poInDS_ == nullptr) {
            setErrorMsg(ERR_OPEN_DATASET_MSG);
            return false;
        }

        imgXSize_ = poInDS_->GetRasterXSize();
        imgYSize_ = poInDS_->GetRasterYSize();
        imgBandCount_ = poInDS_->GetRasterCount();

        // 创建输出图像，输出图像是1个波段
        poOutDS_ = poDriver->Create(outFile_.c_str(), imgXSize_, imgYSize_, 1, GDT_Float32, nullptr);
        if (poOutDS_ == nullptr) {
            GDALClose((GDALDatasetH)poOutDS_);
            setErrorMsg(ERR_CREATE_DATASET_MSG);
            return false;
        }

        // 获取输入图像仿射变换参数
        double adfGeotransform[6] = { 0 };
        poInDS_->GetGeoTransform(adfGeotransform);
        // 设置输出图像仿射变换参数，与原图一致
        poOutDS_->SetGeoTransform(adfGeotransform);

        // 获取输入图像空间参考
        const char* pszProj = poInDS_->GetProjectionRef();
        // 设置输出图像空间参考，与原图一致
        poOutDS_->SetProjection(pszProj);

        pMean_ = new double[imgBandCount_]{};
        pCovariance_ = new double[imgBandCount_*imgBandCount_]{};
    }

    bool RXAnomalyDetection::run() {
        if (!init()) {
            return false;
        }

        switch (poInDS_->GetRasterBand(1)->GetRasterDataType()) {
            case GDT_Byte:
                return runCore<unsigned char>();
                break;

            case GDT_UInt16:
                return runCore<unsigned short>();
                break;

            case GDT_Int16:
                return runCore<short>();
                break;

            case GDT_UInt32:
                return runCore<unsigned int>();
                break;

            case GDT_Int32:
                return runCore<int>();
                break;

            case GDT_Float32:
                return runCore<float>();
                break;

            case GDT_Float64:
                return runCore<double>();
                break;
            default:
                setErrorMsg(ERR_UNKNOWN_TYPE_MSG);
                return false;
        }
    }

    template <typename T>
    bool RXAnomalyDetection::runCore() {

        {
            // step1: 统计协方差矩阵

            ImgTool::MpComputeStatistics stats(poInDS_);
            if (progress_) stats.setProgress(progress_, std::placeholders::_1);

            double *stdDev = new double[imgBandCount_]{};
            if (!stats.run<T>(pMean_, stdDev, pCovariance_)) {
                setErrorMsg(ERR_COMPUTE_COVRIANCE_MSG);
                return false;
            }

            ImgTool::ReleaseArray(stdDev);
        }

        // step2: 计算协方差矩阵的逆矩阵和均值向量
        MatTool::ExtMatrixd matCovariance(pCovariance_, imgBandCount_, imgBandCount_);
//        if (!MatTool::isInvertible(matCovariance)) {
//            setErrorMsg(ERR_MAT_NOT_INVERSE_MSG);
//            return false;
//        }

        MatTool::ExtVectord matMean(pMean_, imgBandCount_);

        MatTool::Matrixd matCovarianceInv(imgBandCount_, imgBandCount_);
        matCovarianceInv = matCovariance.inverse();

        switch (rxdType_) {
            case RXD:
            {
                // 为了便于 RXD 内部的计算，此处转换一下
                double *pCovarInv = new double[imgBandCount_*imgBandCount_]{};
                for (int i = 0; i < imgBandCount_; i++) {
                    for (int j = 0; j < imgBandCount_; j++) {
                        pCovarInv[j*imgBandCount_ + i] = matCovarianceInv(i, j);
                    }
                }

                // 使用多线程处理
                ImgTool::Mp::MpSingleMultiModel<T> mpRxd(4, 4, poInDS_);
                if (progress_) mpRxd.setProgress(progress_,
                        std::placeholders::_1); // , "Anomaly Detection(RXD)"

                for (int i = 0; i < 4; i++) {
                    mpRxd.addProcessBlockData(std::bind( [this, &pCovarInv] (ImgTool::ImgBlockData<T> &data) {
                        float *pOutBuf = new float[data.bufXSize()*data.bufYSize()]{};

                        int size = data.spatial().xSize()*data.spatial().ySize();
                        for (int j = 0; j < size; j++) {
                            T *pTmp = data.bufData() + j*imgBandCount_;

                            for (int i = 0; i < imgBandCount_; i++) {
                                double *pCovarInvTmp = pCovarInv + i*imgBandCount_;

                                double temp = 0;
                                for (int k = 0; k < imgBandCount_; k++) {
                                    temp += (pTmp[k] - pMean_[k]) * pCovarInvTmp[k];
                                }

                                pOutBuf[j] += temp*(pTmp[i] - pMean_[i]);
                            } // end i

                        } // end j

                        // 输出文件
                        // TODO: 需要上锁吗？
                        bool ret = poOutDS_->RasterIO(GF_Write, data.spatial().xOff(), data.spatial().yOff(),
                                data.spatial().xSize(), data.spatial().ySize(), pOutBuf,
                                data.spatial().xSize(), data.spatial().ySize(), GDT_Float32, 1,
                                0, 0, 0, 0);
                        if (!ret) {
                            setErrorMsg("写数据失败");
                            return false;
                        }

                        ImgTool::ReleaseArray(pOutBuf);
                    }, std::placeholders::_1));
                }

                // step 3: 启动各个处理线程，并同步等待处理结果
                // 阻塞再此，直至所有线程结束
                mpRxd.run();

                ImgTool::ReleaseArray(pCovarInv);
                break;
            }

            case UTD:
                break;
            case RXD_UTD:
                break;
        }

        GDALClose((GDALDatasetH)poInDS_);
        GDALClose((GDALDatasetH)poOutDS_);

        return true;
    }


} // naspace ImgAlgo

