//
// Created by penglei on 18-9-9.
//

#include "test_add.h"
#include "gdal_priv.h"
#include "../imgtools/imgtool_mpsingmultmodel.hpp"

test_add::test_add(const std::string &strInFile,
        const std::string &strOutFile)
        : m_strInFile(strInFile), m_strOutFile(strOutFile) {
}

bool test_add::run() {
    //注册GDAL驱动
    GDALAllRegister();

    //获取输出图像驱动
    GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName("ENVI");
    if (poDriver == nullptr)   //输出文件格式错误
        return false;

    //打开输入图像
    poSrcDS = (GDALDataset*)GDALOpen(m_strInFile.c_str(), GA_ReadOnly);
    if (poSrcDS == nullptr)    //输入文件打开失败
        return false;

    //获取输入图像的宽高波段书
    int nXSize = poSrcDS->GetRasterXSize();
    int nYSize = poSrcDS->GetRasterYSize();
    int nBands = poSrcDS->GetRasterCount();

    //获取输入图像仿射变换参数
    double adfGeotransform[6] = { 0 };
    poSrcDS->GetGeoTransform(adfGeotransform);
    //获取输入图像空间参考
    const char* pszProj = poSrcDS->GetProjectionRef();

    GDALRasterBand *poBand = poSrcDS->GetRasterBand(1);
    if (poBand == nullptr)    //获取输入文件中的波段失败
    {
        GDALClose((GDALDatasetH)poSrcDS);
        return false;
    }

    //创建输出图像，输出图像是1个波段
    poDstDS = poDriver->Create(m_strOutFile.c_str(), nXSize, nYSize, 1, GDT_Byte, nullptr);
    if (poDstDS == nullptr)    //创建输出文件失败
    {
        GDALClose((GDALDatasetH)poSrcDS);
        return false;
    }

    //设置输出图像仿射变换参数，与原图一致
    poDstDS->SetGeoTransform(adfGeotransform);
    //设置输出图像空间参考，与原图一致
    poDstDS->SetProjection(pszProj);

    //////////////////////////////////////////////////////////////////////////////////////////////
//    ImgTool::ImgBlockProcess<float> blockProcess(poSrcDS, 128, ImgTool::ImgBlockType::IBT_SQUARE);
//    ImgTool::ImgBlockData<float> &data = blockProcess.data();
//    int bandCount = data.blkSpectralSubset().spectralCount();
//
//    // todo 可设置一些参数，比如分配输出缓冲区
//    unsigned char *pOutBuf = new unsigned char[data.bufXSize()*data.bufYSize()]{};
//
//    blockProcess.processBlockData([&data, this, pOutBuf, &bandCount]{
//         int xOff = data.blkEnvelope().xOff();
//         int yOff = data.blkEnvelope().yOff();
//         int xSize = data.blkEnvelope().xSize();
//         int ySize = data.blkEnvelope().ySize();
//
//         // todo 核心算法
//         int elemSize = xSize*ySize;
//         for (int j = 0; j < elemSize; j++) {
//             float *buf = data.bufData() + j*bandCount;
//
//             double tmp = 0;
//             for (int i = 0; i < bandCount; i++) {
//                 tmp += buf[i];
//             }
//
//             pOutBuf[j] = static_cast<unsigned char>(tmp);
//         }
//
//         this->poDstDS->RasterIO(GF_Write, xOff, yOff, xSize, ySize, pOutBuf, xSize, ySize,
//                 GDT_Byte, 1, nullptr, 0, 0, 0, nullptr);
//
//         std::cout << "<" << xOff << ", " << yOff << ", " << xSize << ", " << ySize << ">\n";
//    });
    //////////////////////////////////////////////////////////////////////////////////////////////

    {
        std::cout << "Begin MpSingleMultiModel\n" << std::endl;

        // $1 测试 MpSingleMultiModel
        // 通过 lambda 调用
//        ImgTool::Mp::MpSingleMultiModel<float> mp(4, 4, poSrcDS, 128, ImgTool::IBT_SQUARE);
//
//        for (int i = 0; i < 4; i++) {
//            //unsigned char *pOutBuf = new unsigned char[]{};
//
//            mp.addProcessBlockData(std::bind(
//                    [&mp, this](ImgTool::ImgBlockData<float> &data/*,...*/) {
//                std::cout << "consume the " << mp.bufQueue().consumedItemCount_ << " item" << std::endl;
//                std::cout << "<" << data.spatial().xOff() << ", "
//                    << data.spatial().yOff() << ", "
//                    << data.spatial().xSize() << ", "
//                    << data.spatial().ySize() << ">\n\n";
//                },
//                std::placeholders::_1));
//        }
//
//        mp.run();
        // $1

        // $2 通过 函数调用
        ImgTool::Mp::MpSingleMultiModel<float> mp(1, 2, poSrcDS, 128, ImgTool::IBT_SQUARE);

        for (int i = 0; i < 1; i++) {
            //unsigned char *pOutBuf = new unsigned char[]{};

            mp.addProcessBlockData(std::bind(&test_add::processDataCore<float>, this,
                    std::placeholders::_1));
        }

        mp.run();

        // $2

        std::cout << "\nEnd MpSingleMultiModel\n" << std::endl;
    }



    GDALClose((GDALDatasetH)poSrcDS);
    GDALClose((GDALDatasetH)poDstDS);
    return true;
}

template <typename T>
void test_add::processDataCore(ImgTool::ImgBlockData<T> &data/*,...*/) {
    std::cout << "<" << data.spatial().xOff() << ", "
              << data.spatial().yOff() << ", "
              << data.spatial().xSize() << ", "
              << data.spatial().ySize() << ">\n\n";
}
