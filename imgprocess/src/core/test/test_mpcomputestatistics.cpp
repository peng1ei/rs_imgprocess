//
// Created by penglei on 18-9-11.
//

#include "test_mpcomputestatistics.h"
#include "imgtool_mpcomputestatistics.hpp"

bool Test_MpComputeStatistics::run() {
    //注册GDAL驱动
    GDALAllRegister();

    //获取输出图像驱动
    GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName("ENVI");
    if (poDriver == nullptr)   //输出文件格式错误
        return false;

    //打开输入图像
    GDALDataset *poSrcDS = (GDALDataset*)GDALOpen(file_.c_str(), GA_ReadOnly);
    if (poSrcDS == nullptr)    //输入文件打开失败
        return false;

    //获取输入图像的宽高波段书
    int nXSize = poSrcDS->GetRasterXSize();
    int nYSize = poSrcDS->GetRasterYSize();
    int nBands = poSrcDS->GetRasterCount();

    double *mean = new double[nBands]{};
    double *stdDev = new double[nBands]{};
    double *covraiance = new double[nBands*nBands]{};

    ImgTool::MpComputeStatistics mps(poSrcDS, 128);
    ImgTool::ProgressTerm term;
    mps.setProgress(term, std::placeholders::_1, "compute statistics");

    if (mps.run<float>(mean, stdDev, covraiance)) {
//        std::cout << "\n----------------------mean-------------------\n";
//        for (int i = 0; i < nBands; i++) {
//            std::cout << mean[i] << std::endl;
//        }
//        std::cout << "\n----------------------std dev-----------------\n";
//        for (int i = 0; i < nBands; i++) {
//            std::cout << stdDev[i] << std::endl;
//        }
//        std::cout << "\n";
    }

    delete[](mean);
    delete[](stdDev);
    delete[](covraiance);

    return true;
}
