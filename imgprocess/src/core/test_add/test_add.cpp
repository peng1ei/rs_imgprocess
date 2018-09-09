//
// Created by penglei on 18-9-9.
//

#include "test_add.h"
#include "gdal_priv.h"
#include "../imgtools/imgtool_imgblockprocess.hpp"

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

    ImgTool::ImgBlockProcess<float> blockProcess(poSrcDS);
    ImgTool::ImgBlockData<float> &data = blockProcess.data();
    unsigned char *pOutBuf = new unsigned char[data.bufXSize()*data.bufYSize()]{};

    blockProcess.processBlockData([this, &data, pOutBuf]{
         int xOff = data.blkEnvelope().xOff();
         int yOff = data.blkEnvelope().yOff();
         int xSize = data.blkEnvelope().xSize();
         int ySize = data.blkEnvelope().ySize();
         int bandCount = data.blkSpectralSubset().spectralCount();

         int elemSize = xSize*ySize;
         for (int j = 0; j < elemSize; j++) {
             float *buf = data.bufData() + j*bandCount;

             double tmp = 0;
             for (int i = 0; i < bandCount; i++) {
                 tmp += buf[i];
             }

             pOutBuf[j] = static_cast<unsigned char>(tmp);
         }

         this->poDstDS->RasterIO(GF_Write, xOff, yOff, xSize, ySize, pOutBuf, xSize, ySize,
                 GDT_Byte, 1, nullptr, 0, 0, 0, nullptr);
    });

    GDALClose((GDALDatasetH)poSrcDS);
    GDALClose((GDALDatasetH)poDstDS);
    return true;
}
