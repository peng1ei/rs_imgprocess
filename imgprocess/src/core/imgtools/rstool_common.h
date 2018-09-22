//
// Created by penglei on 18-9-21.
//

#ifndef IMGPROCESS_RSTOOL_COMMON_H
#define IMGPROCESS_RSTOOL_COMMON_H

#include "gdal/gdal.h"
#include "gdal/gdal_priv.h"
#include <vector>
#include <type_traits>

namespace RSTool {

    template <typename T>
    inline void ReleaseArray(T *pointer) {
        if (pointer) {
            delete []pointer;
            pointer = nullptr;
        }
    }

    // 空间尺寸
    struct SpatialDims {
        SpatialDims(int xOff, int yOff, int xSize, int ySize)
                : xOff_(xOff), yOff_(yOff), xSize_(xSize), ySize_(ySize) {}

        int xOff() const { return xOff_; }
        void xOff(int value) { xOff_ = value; }

        int yOff() const { return yOff_; }
        void yOff(int value) { yOff_ = value; }

        int xSize() const {return xSize_; }
        void xSize(int value) { xSize_ = value; }

        int ySize() const { return ySize_; }
        void ySize(int value) { ySize_ = value; }

    protected:
        // 偏移量从 0 开始
        int xOff_;
        int yOff_;
        int xSize_;
        int ySize_;
    };

    // 光谱尺寸
    struct SpectralDimes {
        SpectralDimes(int bandCount) : bands_(bandCount) {
            int i = 0;
            for (auto &value : bands_) {
                value = ++i;
            }
        }

        SpectralDimes(const std::vector<int> &bands)
                : bands_(bands) {}

        //std::vector<int>& bands() { return bands_; }
        //const std::vector<int>& bands() const { return bands_; }

        int bandCount() { return static_cast<int>(bands_.size()); }
        int* bandMap() { return bands_.data(); }

    protected:
        // 波段索引从 1 开始
        std::vector<int> bands_;
    };

    // 数据的尺寸（包括空间维和光谱维）
    struct DataDims : public SpatialDims, public SpectralDimes {
        // 用于处理全波段
        DataDims(int xOff, int yOff, int xSize, int ySize, int bandCount)
                : SpatialDims(xOff, yOff, xSize, ySize), SpectralDimes(bandCount) {
        }

        // 处理波段子集
        DataDims(int xOff, int yOff, int xSize, int ySize, const std::vector<int> &bands)
                : SpatialDims(xOff, yOff, xSize, ySize), SpectralDimes(bands) {}

        DataDims(const SpatialDims &spatDims, const SpectralDimes &specDims)
                : SpatialDims(spatDims), SpectralDimes(specDims) {}

        DataDims(const SpatialDims &spatDims, int bandCount)
                : SpatialDims(spatDims), SpectralDimes(bandCount) {}

        int elemCount() const { return xSize_*ySize_*bands_.size();}
        void updateSpatial(int xOff, int yOff, int xSize, int ySize) {
            xOff_ = xOff;
            yOff_ = yOff;
            xSize_ = xSize;
            ySize_ = ySize;
        }
    };

    // 影像文件在磁盘中的存储格式或数据在内存中的组织的方式
    enum class Interleave : char {
        BSQ,
        BIL,
        BIP
    };

    // 数据块，里面存放空间和光谱信息以及数据内容
    template <typename T>
    struct DataChunk {
        DataChunk(int xOff, int yOff, int xSize, int ySize, int bandCount,
                  Interleave intl = Interleave::BIP)
                : dims_(xOff, yOff, xSize, ySize, bandCount),
                  intl_(intl) {
            allocMemory();
        }

        DataChunk(int xOff, int yOff, int xSize, int ySize,
                  const std::vector<int> &bands,
                  Interleave intl = Interleave::BIP)
                : dims_(xOff, yOff, xSize, ySize, bands),
                  intl_(intl) {
            allocMemory();
        }

        DataChunk(const SpatialDims &spatDims, const SpectralDimes &specDims,
                Interleave intl = Interleave::BIP)
                : dims_(spatDims, specDims), intl_(intl) {
            allocMemory();
        }

        // 用于全波段处理
        DataChunk(const SpatialDims &spatDims, int bandCount,
                  Interleave intl = Interleave::BIP)
                : dims_(spatDims, bandCount), intl_(intl) {
            allocMemory();
        }

        DataChunk(const DataDims &dims, Interleave intl = Interleave::BIP)
                : dims_(dims), intl_(intl) {
            allocMemory();
        }

        DataChunk(const DataChunk &other)
            : dims_(other.dims_), intl_(other.intl_){
            allocMemory();
            mempcpy(data_, other.data_, sizeof(T)*dims_.elemCount());
        }

        DataChunk& operator= (const DataChunk &other) {
            if (this == &other) {
                return *this;
            }

            ReleaseArray(data_);

            dims_ = other.dims_;
            intl_ = other.intl_;
            allocMemory();
            mempcpy(data_, other.data_, sizeof(T)*dims_.elemCount());
        }

        // 移动构造函数
        DataChunk(DataChunk &&rother) noexcept
            : dims_(rother.dims_), intl_(rother.intl_) {

            // 偷取
            data_ = rother.data_;
            rother.data_ = nullptr;
        }

        // 移动赋值函数
        DataChunk& operator= (DataChunk &&rother) noexcept {
            if (this == &rother) {
                return *this;
            }

            ReleaseArray(data_);
            dims_ = rother.dims_;
            intl_ = rother.intl_;
            data_ = rother.data_;
            rother.data_ = nullptr;
        }

        ~DataChunk() {
            ReleaseArray(data_);
        }

    public:
        const DataDims &dims() const { return dims_; }
        const Interleave &interleave() const { return intl_; }
        T* data() { return data_; }

        void update(int xOff, int yOff, int xSize, int ySize, T *data) {
            dims_.updateSpatial(xOff, yOff, xSize, ySize);
            mempcpy(data_, data, sizeof(T)*dims_.elemCount());
        }

        void replace(int xOff, int yOff, int xSize, int ySize, T *data) {
            ReleaseArray(data_);
            dims_.updateSpatial(xOff, yOff, xSize, ySize);
            data_ = data;
            data = nullptr;
        }

    private:
        void allocMemory() {
            data_ = new T[dims_.elemCount()]{};
        }

    private:
        DataDims dims_;
        Interleave intl_;
        T *data_;
    };

    // 用于将内置类型转换为 GDALDataType
    template <typename T>
    inline GDALDataType toGDALDataType() {
        if ( std::is_same<T, unsigned char>::value ) {
            return GDALDataType::GDT_Byte;
        } else if ( std::is_same<T, unsigned short>::value ) {
            return GDALDataType::GDT_UInt16;
        } else if ( std::is_same<T, short>::value ) {
            return GDALDataType::GDT_Int16;
        } else if ( std::is_same<T, unsigned int>::value ) {
            return GDALDataType::GDT_UInt32;
        } else if ( std::is_same<T, int>::value ) {
            return GDALDataType::GDT_Int32;
        } else if ( std::is_same<T, float>::value ) {
            return GDALDataType::GDT_Float32;
        } else if ( std::is_same<T, double>::value ) {
            return GDALDataType::GDT_Float64;
        }
    }

    // 仿函数，用于读取分块数据
    template <typename T>
    class ReadDataChunk {
    public:
        ReadDataChunk(GDALDataset *dataset) {
            // todo 能否在构造函数中抛出异常 ???
            if (!dataset)
                throw std::runtime_error("GDALDataset is nullptr.");

            dataset_ = dataset;
            dataType_ = toGDALDataType<T>();
        }

        ReadDataChunk(GDALDataset *dataset, const SpectralDimes &specDims,
                Interleave intl = Interleave::BIP)
            : specDims_(specDims), intl_(intl) {

            // todo 能否在构造函数中抛出异常 ???
            if (!dataset)
                throw std::runtime_error("GDALDataset is nullptr.");

            dataset_ = dataset;
            dataType_ = toGDALDataType<T>();
            bandCount_ = specDims_.bandCount();
            bandMap_ = specDims_.bandMap();
        }

        // 全波段处理
        ReadDataChunk(GDALDataset *dataset, int bandCount,
                      Interleave intl = Interleave::BIP)
                : bandCount_(bandCount), bandMap_(nullptr), intl_(intl), specDims_(0){

            // todo 能否在构造函数中抛出异常 ???
            if (!dataset)
                throw std::runtime_error("GDALDataset is nullptr.");

            dataset_ = dataset;
            dataType_ = toGDALDataType<T>();
        }

        bool operator() (DataChunk<T> &data) {
            int xOff = data.dims().xOff();
            int yOff = data.dims().yOff();
            int xSize = data.dims().xSize();
            int ySize = data.dims().ySize();

            switch (data.interleave()) {
                case Interleave::BIP :
                {
                    // todo 将全波段处理和部分波段处理分开
                    if ( CPLErr::CE_Failure == dataset_->RasterIO(GF_Read,
                            xOff, yOff, xSize, ySize, data.data(), xSize, ySize,
                            dataType_, data.dims().bandCount(), data.dims().bandMap(),
                            sizeof(T)*data.dims().bandCount(),
                            sizeof(T)*data.dims().bandCount()*xSize,
                            sizeof(T)) ) {
                        return false;
                    }
                    break;
                }

                case Interleave::BSQ :
                {
                    if ( CPLErr::CE_Failure == dataset_->RasterIO(GF_Read,
                            xOff, yOff, xSize, ySize, data.data(), xSize, ySize,
                            dataType_, data.dims().bandCount(), data.dims().bandMap(),
                            0, 0, 0) ) {
                        return false;
                    }
                    break;
                }

                case Interleave::BIL :
                {
                    if ( CPLErr::CE_Failure == dataset_->RasterIO(GF_Read,
                            xOff, yOff, xSize, ySize, data.data(), xSize, ySize,
                            dataType_, data.dims().bandCount(), data.dims().bandMap(),
                            sizeof(T),
                            sizeof(T)*data.dims().bandCount()*xSize,
                            sizeof(T)*xSize) ) {
                        return false;
                    }
                    break;
                }


            }// end switch

            return true;
        }

        bool operator() (int xOff, int yOff, int xSize, int ySize, T *data) {
            switch (intl_) {
                case Interleave::BIP :
                {
                    // todo 将全波段处理和部分波段处理分开
                    if ( CPLErr::CE_Failure == dataset_->RasterIO(GF_Read,
                            xOff, yOff, xSize, ySize, data, xSize, ySize,
                            dataType_, bandCount_, bandMap_,
                            sizeof(T)*bandCount_,
                            sizeof(T)*bandCount_*xSize,
                            sizeof(T)) ) {
                        return false;
                    }
                    break;
                }

                case Interleave::BSQ :
                {
                    if ( CPLErr::CE_Failure == dataset_->RasterIO(GF_Read,
                            xOff, yOff, xSize, ySize, data, xSize, ySize,
                            dataType_, bandCount_, bandMap_,
                            0, 0, 0) ) {
                        return false;
                    }
                    break;
                }

                case Interleave::BIL :
                {
                    if ( CPLErr::CE_Failure == dataset_->RasterIO(GF_Read,
                            xOff, yOff, xSize, ySize, data, xSize, ySize,
                            dataType_, bandCount_, bandMap_,
                            sizeof(T),
                            sizeof(T)*bandCount_*xSize,
                            sizeof(T)*xSize) ) {
                        return false;
                    }
                    break;
                }

            }// end switch

            return true;
        }

    private:
        GDALDataset *dataset_;
        GDALDataType dataType_;
        SpectralDimes specDims_;
        Interleave intl_;
        int bandCount_;
        int *bandMap_;
    };


} // namespace RSTool

#endif //IMGPROCESS_RSTOOL_COMMON_H