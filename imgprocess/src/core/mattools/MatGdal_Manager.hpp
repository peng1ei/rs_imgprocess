//
// Created by penglei on 18-9-30.
//

#ifndef IMGPROCESS_MATGDAL_MANAGER_HPP
#define IMGPROCESS_MATGDAL_MANAGER_HPP

#include <vector>
#include <string>
#include <memory>
#include "rstool_common.h"

using namespace RSTool;

namespace MatGdal {
// $1
// 处理的时候全部用double，保存出去的时候进行数据类型转换，进行相应的计算
#define SwitchGDALTypeProcess(outGdt) switch(outGdt) {\
    case GDALDataType::GDT_Byte:\
        return commonProcess<unsigned char>();\
    case GDALDataType::GDT_UInt16:\
        return commonProcess<unsigned short>();\
    case GDALDataType::GDT_Int16:\
        return commonProcess<short>();\
    case GDALDataType::GDT_UInt32:\
        return commonProcess<unsigned int>();\
    case GDALDataType::GDT_Int32:\
        return commonProcess<int>();\
    case GDALDataType::GDT_Float32:\
        return commonProcess<float>();\
    case GDALDataType::GDT_Float64:\
        return commonProcess<double>();\
    default:\
        return false;\
    }
// $1

    using namespace Mat;
    //using BandMap = std::vector<int>;

    struct BandMap {
        BandMap() {}
        BandMap(int count)
            : bandMap_(count) {
            int i = 0;
            for (auto &val : bandMap_) {
                val = ++i;
            }
        }
        BandMap(std::initializer_list<int> list) : bandMap_(list) {}

        int size() const { return bandMap_.size(); }

        int* data() { return bandMap_.data(); }
        const int* data() const { return bandMap_.data(); }

    private:
        std::vector<int> bandMap_;
    };

    enum class BlockType : char {
        SQUARE,
        LINE
    };

    class MatGdalManager;
    using MatGdalManagerPtr = std::shared_ptr<MatGdalManager>;

    MatGdalManagerPtr makeMatGdalManager();
    class MatGdalManager{
        friend MatGdalManagerPtr makeMatGdalManager();
    protected:
        MatGdalManager()
            : xBlkSize_(128), yBlkSize_(128), memIntl_(Interleave::BIP),
            blkType_(BlockType::SQUARE), ds_(nullptr) {
            std::cout << "MatGdalManager()\n";
        }

        ~MatGdalManager() {
            std::cout << "~MatGdalManager()\n";
            GDALClose((GDALDatasetH)ds_);
            ds_ = nullptr;
        }

    public:

        bool openDataset(const char *pszFile, GDALAccess eAccess = GA_ReadOnly) {
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
            imgBandCount_ = ds_->GetRasterCount();
            if (blkType_ == BlockType::LINE)
                xBlkSize_ = xImgSize_;

            block();
            return true;
        }
        bool createDataset(const char *pszFile, const char *driverName,
                           int nXSize, int nYSize, int nBands,
                           GDALDataType eType, char ** papszOptions) {
            GDALAllRegister();
            GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName(driverName);
            if (poDriver == nullptr)
                return false;

            ds_ = poDriver->Create(pszFile, nXSize, nYSize, nBands, eType, papszOptions);
            if (ds_ == nullptr) {
                return false;
            }

            gdt_ = eType;
            xImgSize_ = nXSize;
            yImgSize_ = nYSize;
            imgBandCount_ = nBands;
            if (blkType_ == BlockType::LINE)
                xBlkSize_ = xImgSize_;

            block();
            return true;
        }

        const char* getProjectionRef() {
            if (ds_ == nullptr)
                return nullptr;
            return ds_->GetProjectionRef();
        }
        bool setProjection(const char * proj) {
            if (ds_ == nullptr)
                return false;

            ds_->SetProjection(proj);
            return true;
        }

        bool getGeoTransform(double * geoTrans) {
            if (ds_ == nullptr)
                return false;
            ds_->GetGeoTransform(geoTrans);
            return true;
        }
        bool setGeoTransform(double * geoTrans) {
            if (ds_ == nullptr)
                return false;
            ds_->SetGeoTransform(geoTrans);
            return true;
        }

        std::unique_ptr<double> getGeoTransform() {
            if (ds_ == nullptr)
                return nullptr;

            auto geoTrans = std::unique_ptr<double>(new double[6]{0});
            ds_->GetGeoTransform(geoTrans.get());
            return geoTrans;
        }
        bool setGeoTransform(std::unique_ptr<double> geoTrans) {
            if (ds_ == nullptr)
                return false;
            ds_->SetGeoTransform(geoTrans.get());
            return true;
        }

        void resetBlock(int newBlkSize, const BlockType &newBlkType = BlockType::SQUARE) {
            yBlkSize_ = newBlkSize;
            newBlkType == BlockType::LINE ?
                xBlkSize_ = xImgSize_ : xBlkSize_ = newBlkSize;
            block();
        }
        void setMemInterleave(const Interleave &intl) {
            memIntl_ = intl;
            if (intl == Interleave::BIL) memIntl_ = Interleave::BIP;
        }

    protected:
        // TODO 放入保护
        MatrixdPtr readDataChunk(bool isBlock,
                int xBlkOff, int yBlkOff, const BandMap &bands) {
            int xImgOff, yImgOff, xBlkSize, yBlkSize;
            if (isBlock) {
                if (0 > xBlkOff || xBlkOff >= xBlkNum_ ||
                    0 > yBlkOff || yBlkOff >= yBlkNum_)
                    return nullptr;

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

            int bandCount = bands.size();
            switch (memIntl_) {
                case Interleave::BIP: {
                    // 单波段
                    // **************xBlkSize*****************
                    // *                                     *
                    // y                                     *
                    // B                                     *
                    // l                                     *
                    // k                                     *
                    // S                                     *
                    // i                                     *
                    // z                                     *
                    // e                                     *
                    // *                                     *
                    // ***************************************
                    //
                    // 多波段
                    // ******************************************
                    // * * band *
                    // *  *     *  b1 b2 b3 b4 b5 b6 b7 b8 b9 ...
                    // * elem * *
                    // ******************************************
                    // *   e0   *
                    // *   e1   *
                    // *   e2   *
                    // *   e3   *
                    // *   e4   *
                    // *   e5   *
                    // *   e6   *
                    // *   ...  *
                    // ******************************************
                    int matXSize = bandCount;
                    int matYSize = xBlkSize*yBlkSize;
                    if (bandCount == 1) {
                        matXSize = xBlkSize;
                        matYSize = yBlkSize;
                    }

                    auto dataPtr = MatrixdPtr(
                            new Mat::Matrixd(matYSize, matXSize), [](Mat::Matrixd *p){
                                std::cout << "~Matrixd()" << std::endl;
                                delete p;
                            });

                    if (CPLErr::CE_Failure == ds_->RasterIO(GF_Read, xImgOff, yImgOff, xBlkSize, yBlkSize,
                            (*dataPtr).data(), xBlkSize, yBlkSize, GDT_Float64,
                            bandCount, const_cast<int*>(bands.data()),
                            sizeof(double)*bandCount,
                            sizeof(double)*bandCount*xBlkSize,
                            sizeof(double))) {
                        return nullptr;
                    }
                    return std::move(dataPtr);
                }// end BIP

                case Interleave::BSQ: {
                    // 单波段
                    // **************xBlkSize*****************
                    // *                                     *
                    // y                                     *
                    // B                                     *
                    // l                                     *
                    // k                                     *
                    // S                                     *
                    // i                                     *
                    // z                                     *
                    // e                                     *
                    // *                                     *
                    // ***************************************
                    //
                    // 多波段
                    // ******************************************
                    // * * elem *
                    // *  *     *  e0 e1 b2 e3 e4 e5 e6 e7 e8 ...
                    // * band * *
                    // ******************************************
                    // *   b1   *
                    // *   b2   *
                    // *   b3   *
                    // *   b4   *
                    // *   b5   *
                    // *   b6   *
                    // *   b7   *
                    // *   ...  *
                    // ******************************************
                    int matXSize = xBlkSize*yBlkSize;
                    int matYSize = bandCount;
                    if (bandCount == 1) {
                        matXSize = xBlkSize;
                        matYSize = yBlkSize;
                    }

                    auto dataPtr = MatrixdPtr(
                            new Mat::Matrixd(matYSize, matXSize), [](Mat::Matrixd *p){
                                std::cout << "~Matrixd()" << std::endl;
                                delete p;
                            });
                    if (CPLErr::CE_Failure == ds_->RasterIO(GF_Read, xImgOff, yImgOff, xBlkSize, yBlkSize,
                            (*dataPtr).data(), xBlkSize, yBlkSize, GDT_Float64,
                            bandCount, const_cast<int*>(bands.data()), 0, 0, 0)) {
                        return nullptr;
                    }
                    return std::move(dataPtr);
                } // end BSQ
            }
        } // end readDataChunk

    public:
        MatrixdPtr readDataChunk(bool isBlock,
                int blkIndex, const BandMap &bands) {
            if (isBlock) {
                int xBlkOff = blkIndex % xBlkNum_;
                int yBlkOff = blkIndex / xBlkNum_;
                return readDataChunk(isBlock, xBlkOff, yBlkOff, bands);
            } else {
                return readDataChunk(isBlock, 0, 0, bands);
            }
        }

        template <typename OutScalar>
        bool writeDataChunk(bool isBlock,
                int xBlkOff, int yBlkOff,
                std::vector<int> &bands,
                MatrixdPtr data) {

            int xImgOff, yImgOff, xBlkSize, yBlkSize;
            if (isBlock) {
                if (0 > xBlkOff || xBlkOff >= xBlkNum_ ||
                    0 > yBlkOff || yBlkOff >= yBlkNum_)
                    return false;

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

            // 将 double 类型数据转换为指定数据类型
            (*data).cast<OutScalar>();

            int bandCount = bands.size();
            switch (memIntl_) {
                case Interleave::BIP: {
                    // 单波段
                    // **************xBlkSize*****************
                    // *                                     *
                    // y                                     *
                    // B                                     *
                    // l                                     *
                    // k                                     *
                    // S                                     *
                    // i                                     *
                    // z                                     *
                    // e                                     *
                    // *                                     *
                    // ***************************************
                    //
                    // 多波段
                    // ******************************************
                    // * * band *
                    // *  *     *  b1 b2 b3 b4 b5 b6 b7 b8 b9 ...
                    // * elem * *
                    // ******************************************
                    // *   e0   *
                    // *   e1   *
                    // *   e2   *
                    // *   e3   *
                    // *   e4   *
                    // *   e5   *
                    // *   e6   *
                    // *   ...  *
                    // ******************************************


                    if (CPLErr::CE_Failure == ds_->RasterIO(GF_Write, xImgOff, yImgOff, xBlkSize, yBlkSize,
                            (*data).data(), xBlkSize, yBlkSize, gdt_,
                            bandCount, bands.data(),
                            sizeof(OutScalar)*bandCount,
                            sizeof(OutScalar)*bandCount*xBlkSize,
                            sizeof(OutScalar))) {
                        return false;
                    }
                    return true;
                }// end BIP

                case Interleave::BSQ: {
                    // 单波段
                    // **************xBlkSize*****************
                    // *                                     *
                    // y                                     *
                    // B                                     *
                    // l                                     *
                    // k                                     *
                    // S                                     *
                    // i                                     *
                    // z                                     *
                    // e                                     *
                    // *                                     *
                    // ***************************************
                    //
                    // 多波段
                    // ******************************************
                    // * * elem *
                    // *  *     *  e0 e1 b2 e3 e4 e5 e6 e7 e8 ...
                    // * band * *
                    // ******************************************
                    // *   b1   *
                    // *   b2   *
                    // *   b3   *
                    // *   b4   *
                    // *   b5   *
                    // *   b6   *
                    // *   b7   *
                    // *   ...  *
                    // ******************************************
                    if (CPLErr::CE_Failure == ds_->RasterIO(GF_Write, xImgOff, yImgOff, xBlkSize, yBlkSize,
                            (*data).data(), xBlkSize, yBlkSize, gdt_,
                            bandCount, bands.data(), 0, 0, 0)) {
                        return false;
                    }
                    return true;
                } // end BSQ
            }
        }

        template <typename OutScalar>
        bool writeDataChunk(bool isBlock,
                int blkIndex, std::vector<int> &bands,
                MatrixdPtr data) {

            if (isBlock) {
                int xBlkOff = blkIndex % xBlkNum_;
                int yBlkOff = blkIndex / xBlkNum_;
                return writeDataChunk<OutScalar>(isBlock, xBlkOff, yBlkOff, bands, data);
            } else {
                return writeDataChunk<OutScalar>(isBlock, 0, 0, bands, data);
            }
        }

        int blkNum() const { return xBlkNum_*yBlkNum_; }
        int xBlkNum() const { return xBlkNum_; }
        int yBlkNum() const { return yBlkNum_; }

        GDALDataType& getGdalDataType() {
            return gdt_;
        }

        int getRasterCount() const { return imgBandCount_; }
        int getRasterXSize() const { return xImgSize_; }
        int getRasterYSize() const { return yImgSize_; }

    private:
        void block() {
            xBlkNum_ = (xImgSize_ + xBlkSize_ - 1) / xBlkSize_;
            yBlkNum_ = (yImgSize_ + yBlkSize_ - 1) / yBlkSize_;
        }

    private:
        // 影像相关信息
        std::string file_;
        GDALDataset *ds_;
        GDALDataType gdt_;  // 影像真实数据类型
        int xImgSize_;
        int yImgSize_;
        int imgBandCount_;

        Interleave memIntl_;   // 数据在内存中的存储方式

        // 分块信息
        BlockType blkType_;
        int xBlkSize_;
        int yBlkSize_;
        int xBlkNum_;
        int yBlkNum_;
    };

    MatGdalManagerPtr makeMatGdalManager() {
        return MatGdalManagerPtr(new MatGdalManager,
                [](MatGdalManager*p){ delete p; });
    }

} // namespace MatGdal

#endif //IMGPROCESS_MATGDAL_MANAGER_HPP
