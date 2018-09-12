//
// Created by penglei on 18-9-11.
//

#include "test_computestatistics.h"
#include "gdal_priv.h"
#include <iostream>
#include <imgtool_progress.hpp>
#include "imgtool_progress.hpp"

bool computeStatistics(const std::string &file) {
    //注册GDAL驱动
    GDALAllRegister();

    //获取输出图像驱动
    GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName("ENVI");
    if (poDriver == NULL)   //输出文件格式错误
        return false;

    //打开输入图像
    GDALDataset *poSrcDS = (GDALDataset*)GDALOpen(file.c_str(), GA_ReadOnly);
    if (poSrcDS == NULL)    //输入文件打开失败
        return false;

    //获取输入图像的宽高波段书
    int nXSize = poSrcDS->GetRasterXSize();
    int nYSize = poSrcDS->GetRasterYSize();
    int nBands = poSrcDS->GetRasterCount();

    //获取输入图像仿射变换参数
    //double adfGeotransform[6] = { 0 };
    //poSrcDS->GetGeoTransform(adfGeotransform);
    //获取输入图像空间参考
    //const char* pszProj = poSrcDS->GetProjectionRef();

    GDALRasterBand *poBand = poSrcDS->GetRasterBand(1);
    if (poBand == NULL)    //获取输入文件中的波段失败
    {
        GDALClose((GDALDatasetH)poSrcDS);
        return false;
    }

    //创建输出图像，输出图像是1个波段
//    GDALDataset *poDstDS = poDriver->Create(pszDstFile, nXSize, nYSize, 1, GDT_Byte, NULL);
//    if (poDstDS == NULL)    //创建输出文件失败
//    {
//        GDALClose((GDALDatasetH)poSrcDS);
//        return false;
//    }
//
//    //设置输出图像仿射变换参数，与原图一致
//    poDstDS->SetGeoTransform(adfGeotransform);
//    //设置输出图像空间参考，与原图一致
//    poDstDS->SetProjection(pszProj);

    int nBlockSize = 128;     //分块大小

    //分配输入分块缓存
    //unsigned char *pSrcData = new unsigned char[nBlockSize*nBlockSize*nBands];
    //分配输出分块缓存
    //unsigned char *pDstData = new unsigned char[nBlockSize*nBlockSize];

    //定义读取输入图像波段顺序
    int *pBandMaps = new int[nBands];
    for (int b = 0; b < nBands; b++)
        pBandMaps[b] = b + 1;

    float *pSrcData = new float[nBlockSize*nBlockSize*nBands]{};
    double *mean = new  double[nBands]{};
    double *stdDev = new double[nBands]{};
    double *covariance = new double[nBands*nBands]{};

    int pos = 0;
    ImgTool::ProgressTerm term;

    int nXNums = nXSize / nBlockSize;
    int nYNums = nYSize / nBlockSize;
    int xL = nXSize % nBlockSize;
    int yL = nYSize % nBlockSize;
    if (xL > 0) nXNums++;
    if (yL > 0) nYNums++;
    int total = nXNums*nYNums;

    //循环分块并进行处理
    for (int i = 0; i < nYSize; i += nBlockSize)
    {
        for (int j = 0; j < nXSize; j += nBlockSize)
        {
            //定义两个变量来保存分块大小
            int nXBK = nBlockSize;
            int nYBK = nBlockSize;

            //如果最下面和最右边的块不够256，剩下多少读取多少
            if (i + nBlockSize > nYSize)     //最下面的剩余块
                nYBK = nYSize - i;
            if (j + nBlockSize > nXSize)     //最右侧的剩余块
                nXBK = nXSize - j;

            //读取原始图像块
            poSrcDS->RasterIO(GF_Read, j, i, nXBK, nYBK, pSrcData, nXBK, nYBK, GDT_Float32,
                    nBands, pBandMaps, sizeof(float)*nBands, sizeof(float)*nBands*nXBK, sizeof(float), NULL);

            //再这里填写你自己的处理算法
            //pSrcData 就是读取到的分块数据，存储顺序为，先行后列，最后波段
            //pDstData 就是处理后的二值图数据，存储顺序为先行后列
            int size = nXBK * nYBK;
            float *pBuf1, *pBuf2;

            for (int i = 0; i < size; i++) {
                int index = i*nBands;
                pBuf1 = pSrcData + index;

                for (int b = 0; b < nBands; b++) {
                    pBuf2 = pSrcData + b;

                    // sum of X: x1 + x2 + x3 + ...
                    mean[b] += pBuf2[index];

                    // sum of X^2: x1^2 + x2^2 + x3^2 + ...
                    stdDev[b] += pBuf2[index]*pBuf2[index];

                    // sum of X*Y: x1*y1 + x2*y2 + x3*y3 + ...
                    double *pCovar = covariance + b*nBands;
                    for (int b1 = b; b1 < nBands; b1++) {
                        pCovar[b1] = pBuf1[b1]*pBuf2[index];
                    }
                }

            }


            term((pos++)*100.0 / total, "single compute covariance");
        }
    }
    term(100, "single compute covariance");

    int size = nXSize*nYSize;
    for (int i = 0; i < nBands; i++) {
        mean[i] /= size;
    }

    // 标准差
    for (int i = 0; i < nBands; i++) {
        stdDev[i] /= size;
        stdDev[i] = std::sqrt(stdDev[i] - mean[i]*mean[i]);
    }

    double *correlation = nullptr;
    // 协方差矩阵和相关系数矩阵
    for (int i = 0; i < nBands; i++) {
        for (int j = 0; j < nBands; j++) {
            int index = i*nBands + j;
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

    //释放申请的内存
    delete[]pSrcData;
    delete[]mean;
    delete[]stdDev;
    delete[]covariance;

    //关闭原始图像和结果图像
    GDALClose((GDALDatasetH)poSrcDS);
    //GDALClose((GDALDatasetH)poDstDS);

    return true;
}
