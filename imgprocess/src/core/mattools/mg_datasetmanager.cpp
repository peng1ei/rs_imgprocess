//
// Created by penglei on 18-10-10.
//

#include "mg_datasetmanager.h"
//#include "gdal_priv.h"
#include <cassert>
#include <type_traits>

namespace Mg {

    MgDatasetManagerPtr makeMgDatasetManager() {
        return MgDatasetManagerPtr(new MgDatasetManager,
                [](MgDatasetManager*p){ delete p; });
    }

    MgDatasetManager::MgDatasetManager()
        : xBlkSize_(128), yBlkSize_(128),
         blkType_(MgBlockType::SQUARE), ds_(nullptr){
    }

    MgDatasetManager::~MgDatasetManager() {
        GDALClose((GDALDatasetH)ds_);
    }

    bool MgDatasetManager::openDataset(const char *pszFile, GDALAccess eAccess) {
        GDALAllRegister();
        ds_ = (GDALDataset*)GDALOpen(pszFile, eAccess);
        if (ds_ == nullptr) {
            return false;
        }

        GDALRasterBand *poBand = ds_->GetRasterBand(1);
        if (poBand == nullptr) {
            return false;
        }

        gdt_ = poBand->GetRasterDataType();
        xImgSize_ = ds_->GetRasterXSize();
        yImgSize_ = ds_->GetRasterYSize();
        bandCount_ = ds_->GetRasterCount();
        if (blkType_ == MgBlockType::LINE)
            xBlkSize_ = xImgSize_;

        block();
        return true;
    }

    bool MgDatasetManager::createDataset(const char *pszFile, const char *driverName,
            int xSize, int ySize, int bands,
            GDALDataType eType, char **papszOptions) {

        GDALAllRegister();
        GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName(driverName);
        if (poDriver == nullptr)
            return false;

        ds_ = poDriver->Create(pszFile, xSize, ySize, bands, eType, papszOptions);
        if (ds_ == nullptr) {
            return false;
        }

        gdt_ = eType;
        xImgSize_ = xSize;
        yImgSize_ = ySize;
        bandCount_ = bands;
        if (blkType_ == MgBlockType::LINE)
            xBlkSize_ = xImgSize_;

        block();
        return true;
    }

    void MgDatasetManager::block() {
        xBlkNum_ = (xImgSize_ + xBlkSize_ - 1) / xBlkSize_;
        yBlkNum_ = (yImgSize_ + yBlkSize_ - 1) / yBlkSize_;
    }

    void MgDatasetManager::resetBlock(int newBlkSize, const Mg::MgBlockType &newBlkType) {
        yBlkSize_ = newBlkSize;
        newBlkType == MgBlockType::LINE ?
                xBlkSize_ = xImgSize_ : xBlkSize_ = newBlkSize;
        block();
    }

    bool MgDatasetManager::readDataChunk(bool isBlock, int xBlkOff, int yBlkOff,
            const Mg::MgBandMap &bands, Mg::MgCube &cube) {
        int xImgOff, yImgOff, xBlkSize, yBlkSize;
        if (isBlock) {
            yImgOff = yBlkOff*yBlkSize_;
            yBlkSize = yBlkSize_;
            if (yImgOff + yBlkSize_ > yImgSize_)
                yBlkSize = yImgSize_ - yImgOff;

            xImgOff = xBlkOff*xBlkSize_;
            xBlkSize = xBlkSize_;
            if (xImgOff + xBlkSize_ > xImgSize_)
                xBlkSize = xImgSize_ - xImgOff;
        } else {
            xImgOff = 0;
            yImgOff = 0;
            xBlkSize = xImgSize_;
            yBlkSize = yImgSize_;
        }

        cube.resize(yBlkSize, xBlkSize, bands.size());
        if (CPLErr::CE_Failure == ds_->RasterIO(GF_Read, xImgOff, yImgOff,
                xBlkSize, yBlkSize,
                cube.data().data(), xBlkSize, yBlkSize, GDT_Float32,
                bands.size(), const_cast<int*>(bands.data()),
                0, 0, 0)) {
            return false;
        }

        return true;
    }

    bool MgDatasetManager::readDataChunk(bool isBlock, int blkIndex,
            const Mg::MgBandMap &bands, Mg::MgCube &cube) {
        if (isBlock) {
            assert(blkIndex >= 0 && blkIndex < xBlkNum_*yBlkNum_);
            int xBlkOff = blkIndex % xBlkNum_;
            int yBlkOff = blkIndex / xBlkNum_;
            return readDataChunk(isBlock, xBlkOff, yBlkOff, bands, cube);
        } else {
            assert(blkIndex == 0);
            return readDataChunk(isBlock, 0, 0, bands, cube);
        }
    }

    bool MgDatasetManager::readDataChunk(int xOff, int yOff, int xSize, int ySize,
            const Mg::MgBandMap &bands, Mg::MgCube &cube) {
        cube.resize(ySize, xSize, bands.size());
        if (CPLErr::CE_Failure == ds_->RasterIO(GF_Read,
                xOff, yOff, xSize, ySize,
                cube.data().data(), xSize, ySize, GDT_Float32,
                bands.size(), const_cast<int*>(bands.data()),
                0, 0, 0)) {
            return false;
        }

        return true;
    }
//
//    template <typename OutScalar>
//    bool MgDatasetManager::writeDataChunk(bool isBlock, int xBlkOff, int yBlkOff,
//            const Mg::MgBandMap &bands, Mg::MgCube &cube) {
//        int xImgOff, yImgOff, xBlkSize, yBlkSize;
//        if (isBlock) {
////            if (0 > xBlkOff || xBlkOff >= xBlkNum_ ||
////                0 > yBlkOff || yBlkOff >= yBlkNum_)
////                return false;
//
//            yImgOff = yBlkOff*yBlkSize_;
//            yBlkSize = cube.height();
////            if (yImgOff + yBlkSize_ > yImgSize_)
////                yBlkSize = yImgSize_ - yImgOff;
//
//            xImgOff = xBlkOff*xBlkSize_;
//            xBlkSize = cube.width();
////            if (xImgOff + xBlkSize_ > xImgSize_)
////                xBlkSize = xImgSize_ - xImgOff;
//        } else {
//            xImgOff = 0;
//            yImgOff = 0;
//            xBlkSize = cube.width();
//            yBlkSize = cube.height();
//        }
//
//        if (!std::is_same<OutScalar, double>::value) {
//            // 将 double 类型数据转换为指定数据类型
//            Mat::Matrix<OutScalar> &&tmp = (cube.data()).cast<OutScalar>();
//            if (CPLErr::CE_Failure == ds_->RasterIO(GF_Write, xImgOff, yImgOff,
//                    xBlkSize, yBlkSize,
//                    tmp.data(), xBlkSize, yBlkSize, gdt_,
//                    bands.size(), const_cast<int*>(bands.data()), 0, 0, 0)) {
//                return false;
//            }
//
//            return true;
//        }
//
//        if (CPLErr::CE_Failure == ds_->RasterIO(GF_Write, xImgOff, yImgOff,
//                xBlkSize, yBlkSize,
//                cube.data().data(), xBlkSize, yBlkSize, gdt_,
//                bands.size(), const_cast<int*>(bands.data()), 0, 0, 0)) {
//            return false;
//        }
//
//        return true;
//    }
//
//    template <typename OutScalar>
//    bool MgDatasetManager::writeDataChunk(bool isBlock, int blkIndex,
//            const Mg::MgBandMap &bands, Mg::MgCube &cube) {
//        if (isBlock) {
//            assert(blkIndex >= 0 && blkIndex < xBlkNum_*yBlkNum_);
//            int xBlkOff = blkIndex % xBlkNum_;
//            int yBlkOff = blkIndex / xBlkNum_;
//            return writeDataChunk<OutScalar>(isBlock, xBlkOff, yBlkOff, bands, cube);
//        } else {
//            assert(blkIndex == 0);
//            return writeDataChunk<OutScalar>(isBlock, 0, 0, bands, cube);
//        }
//    }

    template <typename OutScalar>
    bool MgDatasetManager::writeDataChunk(int xOff, int yOff,
            const MgBandMap &bands, MgCube &cube) {

        if (!std::is_same<OutScalar, double>::value) {
            // 将 double 类型数据转换为指定数据类型
            Mat::Matrix<OutScalar> &&tmp = (cube.data()).cast<OutScalar>();
            if (CPLErr::CE_Failure == ds_->RasterIO(GF_Read,
                    xOff, yOff, cube.width(), cube.height(),
                    tmp.data(), cube.width(), cube.height(), gdt_,
                    bands.size(), const_cast<int*>(bands.data()),
                    0, 0, 0)) {
                return false;
            }

            return true;
        }

        if (CPLErr::CE_Failure == ds_->RasterIO(GF_Read,
                xOff, yOff, cube.width(), cube.height(),
                cube.data().data(), cube.width(), cube.height(), gdt_,
                bands.size(), const_cast<int*>(bands.data()),
                0, 0, 0)) {
            return false;
        }

        return true;
    }

    const char* MgDatasetManager::getProjectionRef() {
        assert(ds_ != nullptr);
        return ds_->GetProjectionRef();
    }

    void MgDatasetManager::setProjection(const char *proj) {
        assert(ds_ != nullptr);
        ds_->SetProjection(proj);
    }

    void MgDatasetManager::getGeoTransform(double *geoTrans) {
        assert(ds_ != nullptr);
        ds_->GetGeoTransform(geoTrans);
    }

    void MgDatasetManager::setGeoTransform(double *geoTrans) {
        assert(ds_ != nullptr);
        ds_->SetGeoTransform(geoTrans);
    }

    std::unique_ptr<double[]> MgDatasetManager::getGeoTransform() {
        assert(ds_ != nullptr);
        auto geoTrans = std::unique_ptr<double[]>(new double[6]{0});
        ds_->GetGeoTransform(geoTrans.get());
        return geoTrans;
    }

    void MgDatasetManager::setGeoTransform(std::unique_ptr<double[]> geoTrans) {
        assert(ds_ != nullptr);
        ds_->SetGeoTransform(geoTrans.get());
    }

}// namespace Mg
