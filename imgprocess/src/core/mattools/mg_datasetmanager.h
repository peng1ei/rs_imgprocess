//
// Created by penglei on 18-10-10.
//

#ifndef IMGPROCESS_MG_DATASETMANAGER_H
#define IMGPROCESS_MG_DATASETMANAGER_H

#include "mg_common.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "mg_matcommon.h"
#include "mg_cube.h"
#include <string>
#include <memory>
#include <future>
#include <thread>
#include <mutex>
#include <condition_variable>

class GDALDataset;

namespace Mg {

    class MgDatasetManager;

    using MgDatasetManagerPtr = std::shared_ptr<MgDatasetManager>;
    MgDatasetManagerPtr makeMgDatasetManager();

    class MgDatasetManager {
        friend MgDatasetManagerPtr makeMgDatasetManager();

    protected:
        MgDatasetManager();
        ~MgDatasetManager();

    public:
        bool openDataset(const char *pszFile, GDALAccess eAccess = GA_ReadOnly);
        bool createDataset(const char *pszFile, const char *driverName,
                int xSize, int ySize, int bands,
                GDALDataType eType, char ** papszOptions);

        void resetBlock(int newBlkSize, const MgBlockType &newBlkType = MgBlockType::SQUARE);
        bool readDataChunk(bool isBlock, int blkIndex, const MgBandMap &bands, MgCube &cube);
        bool readDataChunk(int xOff, int yOff, int xSize, int ySize,
                const MgBandMap &bands, MgCube &cube);

        template <typename OutScalar>
        bool writeDataChunk(bool isBlock, int blkIndex, const MgBandMap &bands, MgCube &cube);

        template <typename OutScalar>
        bool writeDataChunk(int xOff, int yOff, const MgBandMap &bands, MgCube &cube);

        int blkNum() const { return xBlkNum_*yBlkNum_; }
        int xBlkNum() const { return xBlkNum_; }
        int yBlkNum() const { return yBlkNum_; }

        GDALDataType& getGdalDataType() { return gdt_; }
        int getRasterCount() const { return bandCount_; }
        int getRasterXSize() const { return xImgSize_; }
        int getRasterYSize() const { return yImgSize_; }

        const char* getProjectionRef();
        void setProjection(const char * proj);

        void getGeoTransform(double * geoTrans);
        void setGeoTransform(double * geoTrans);

        std::unique_ptr<double[]> getGeoTransform();
        void setGeoTransform(std::unique_ptr<double[]> geoTrans);

    protected:
        bool readDataChunk(bool isBlock,
                int xBlkOff, int yBlkOff, const MgBandMap &bands, MgCube &cube);

        template <typename OutScalar>
        bool writeDataChunk(bool isBlock, int xBlkOff, int yBlkOff,
                const MgBandMap &bands, MgCube &cube);
    private:
        void block();

    private:
        // 影像相关信息
        std::string file_;
        GDALDataset *ds_;
        GDALDataType gdt_;       // 影像真实数据类型
        int xImgSize_;
        int yImgSize_;
        int bandCount_;

        // 分块信息
        MgBlockType blkType_;
        int xBlkSize_;
        int yBlkSize_;
        int xBlkNum_;
        int yBlkNum_;
    };


    template <typename OutScalar>
    bool MgDatasetManager::writeDataChunk(bool isBlock, int xBlkOff, int yBlkOff,
                                          const Mg::MgBandMap &bands, Mg::MgCube &cube) {
            int xImgOff, yImgOff, xBlkSize, yBlkSize;
            if (isBlock) {
//            if (0 > xBlkOff || xBlkOff >= xBlkNum_ ||
//                0 > yBlkOff || yBlkOff >= yBlkNum_)
//                return false;

                    yImgOff = yBlkOff*yBlkSize_;
                    yBlkSize = cube.height();
//            if (yImgOff + yBlkSize_ > yImgSize_)
//                yBlkSize = yImgSize_ - yImgOff;

                    xImgOff = xBlkOff*xBlkSize_;
                    xBlkSize = cube.width();
//            if (xImgOff + xBlkSize_ > xImgSize_)
//                xBlkSize = xImgSize_ - xImgOff;
            } else {
                    xImgOff = 0;
                    yImgOff = 0;
                    xBlkSize = cube.width();
                    yBlkSize = cube.height();
            }

            if (!std::is_same<OutScalar, float>::value) {
                    // 将 double 类型数据转换为指定数据类型
                    Mat::Matrix<OutScalar> &&tmp = (cube.data()).cast<OutScalar>();

                    if (CPLErr::CE_Failure == ds_->RasterIO(GF_Write, xImgOff, yImgOff,
                            xBlkSize, yBlkSize,
                            tmp.data(), xBlkSize, yBlkSize, gdt_,
                            bands.size(), const_cast<int*>(bands.data()), 0, 0, 0)) {
                            return false;
                    }

                    return true;
            }

            if (CPLErr::CE_Failure == ds_->RasterIO(GF_Write, xImgOff, yImgOff,
                    xBlkSize, yBlkSize,
                    cube.data().data(), xBlkSize, yBlkSize, gdt_,
                    bands.size(), const_cast<int*>(bands.data()), 0, 0, 0)) {
                    return false;
            }

            return true;
    }

    template <typename OutScalar>
    bool MgDatasetManager::writeDataChunk(bool isBlock, int blkIndex,
            const Mg::MgBandMap &bands, Mg::MgCube &cube) {
            if (isBlock) {
                assert(blkIndex >= 0 && blkIndex < xBlkNum_*yBlkNum_);
                int xBlkOff = blkIndex % xBlkNum_;
                int yBlkOff = blkIndex / xBlkNum_;
                return writeDataChunk<OutScalar>(isBlock, xBlkOff, yBlkOff, bands, cube);
            } else {
                assert(blkIndex == 0);
                return writeDataChunk<OutScalar>(isBlock, 0, 0, bands, cube);
            }
    }

} // namespace Mg

#endif //IMGPROCESS_MG_DATASETMANAGER_H
