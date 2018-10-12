//
// Created by penglei on 18-10-10.
//

#ifndef IMGPROCESS_MG_DATASETMANAGER_H
#define IMGPROCESS_MG_DATASETMANAGER_H

#include "mg_common.h"
#include "gdal.h"
#include "mg_matcommon.h"
#include "mg_cube.h"
#include <string>
#include <memory>

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

} // namespace Mg

#endif //IMGPROCESS_MG_DATASETMANAGER_H
