//
// Created by penglei on 18-9-8.
//

#ifndef IMG_PROCESS_IMGTOOL_COMMON_HPP
#define IMG_PROCESS_IMGTOOL_COMMON_HPP

#include <stdexcept>
#include <type_traits>
#include <utility>
#include <cstdlib>
#include <vector>
#include <iostream>
#include <functional>
#include "gdal/gdal.h"
#include "gdal/gdal_priv.h"

class GDALDataset;

namespace ImgTool {

    enum ImgInterleaveType {
        IIT_BSQ,
        IIT_BIL,
        IIT_BIP
    };

    enum ImgBlockType {
        IBT_LINE,   /* 以行为基本单位进行分块 */
        IBT_SQUARE  /* 以方形进行分块 */
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

    // 用于块处理时指定块空间范围
    struct ImgBlockRect {
        ImgBlockRect() : xOff_(0), yOff_(0), xSize_(0), ySize_(0) {}
        ImgBlockRect(int xOff, int yOff, int xSize, int ySize) {
            xOff_ = xOff;
            yOff_ = yOff;
            xSize_ = xSize;
            ySize_ = ySize;
        }

        int xOff() const { return xOff_; }
        void xOff(int value) { xOff_ = value; }

        int yOff() const { return yOff_; }
        void yOff(int value) { yOff_ = value; }

        int xSize() const { return xSize_; }
        void xSize(int value) { xSize_ = value; }

        int ySize() const { return ySize_; }
        void ySize(int value) { ySize_ = value; }

        void setRect(int xOff, int yOff, int xSize, int ySize) {
            xOff_ = xOff;
            yOff_ = yOff;
            xSize_ = xSize;
            ySize_ = ySize;
        }

    private:
        int xOff_;   // 起始偏移量从 0 开始
        int yOff_;   // 起始偏移量从 0 开始
        int xSize_;
        int ySize_;
    };

    // todo 是否需要？？？
    struct ImgBlockDims {
        int xSize_;
        int ySize_;
        int bandCount_;
    };

    // 空间处理子集
    struct ImgSpatialSubset {
        ImgSpatialSubset() {}
        ImgSpatialSubset(int xOff, int yOff, int xSize, int ySize)
                : spatial_(xOff, yOff, xSize, ySize) {}

        int xOff() const { return spatial_.xOff(); }
        void xOff(int value) { spatial_.xOff(value); }

        int yOff() const { return spatial_.yOff(); }
        void yOff(int value) { spatial_.yOff(value); }

        int xSize() const { return spatial_.xSize(); }
        void xSize(int value) { spatial_.xSize(value); }

        int ySize() const { return spatial_.ySize(); }
        void ySize(int value) { spatial_.ySize(value); }

        void setRange(int xOff, int yOff, int xSize, int ySize) {
            spatial_.setRect(xOff, yOff, xSize, ySize);
        }

    private:
        ImgBlockRect spatial_;
    };

    // 光谱处理子集
    // 用 std::vector<int> 实现，存储待处理波段的索引号（从 1 开始）
    // vector 的大小表示需处理的波段数
    struct ImgSpectralSubset {
        ImgSpectralSubset() {}
        ImgSpectralSubset(int count) {
            for (int i = 1; i <= count; )
                spectral_.push_back(i++);
        }

        ImgSpectralSubset(int bandCount, int *bandMap) {
            for (int i = 0; i < bandCount; )
                spectral_.push_back(bandMap[i++]);
        }

        int count() { return static_cast<int>(spectral_.size()); }
        int* map() { return spectral_.data(); }

        void addSpectral(int index) {
            spectral_.push_back(index);
        };

    private:
        std::vector<int> spectral_;
    };

    template <typename T>
    struct ImgBlockData {
        ImgBlockData() : m_blkBufData(nullptr) {}

        ImgBlockData(ImgSpatialSubset blkSpat, ImgSpectralSubset blkSpec,
                     int bufXSize, int bufYSize, ImgInterleaveType blkInterl = IIT_BIP)
                : m_spatial(blkSpat), m_spectral(blkSpec),
                  m_blkBufXSize(bufXSize), m_blkBufYSize(bufYSize),
                  m_blkBufInterleave(blkInterl), m_blkBufData(nullptr) {
            allocateBuffer();
        }

        ImgBlockData(const ImgBlockData &other)
                : m_spatial(other.m_spatial),
                  m_spectral(other.m_spectral),
                  m_blkBufInterleave(other.m_blkBufInterleave),
                  m_blkBufData(nullptr) {
            m_blkBufXSize = other.m_blkBufXSize;
            m_blkBufYSize = other.m_blkBufYSize;

            allocateBuffer();
            memcpy(m_blkBufData, other.m_blkBufData, sizeof(T)*bufDims());

            //std::cout << "copy ctor" << std::endl;
        }

        virtual ~ImgBlockData() { freeBuffer(); }

        // todo 移动构造/赋值函数

        ImgBlockData& operator= (const ImgBlockData &other) {
            if (this == &other)
                return *this;

            m_spatial = other.m_spatial;
            m_blkBufInterleave = other.m_blkBufInterleave;
            m_spectral = other.m_spectral;
            m_blkBufXSize = other.m_blkBufXSize;
            m_blkBufYSize = other.m_blkBufYSize;
            //m_blkBufData = nullptr;

            allocateBuffer();
            memcpy(m_blkBufData, other.m_blkBufData, sizeof(T)*bufDims());

            //std::cout << "assignment ctor" << std::endl;
            return *this;
        }

        void init(ImgSpatialSubset &blkSpat, ImgSpectralSubset &blkSpec,
                  int bufXSize, int bufYSize, ImgInterleaveType blkInterl = IIT_BIP) {
            m_spatial = blkSpat;
            m_spectral = blkSpec;
            m_blkBufXSize = bufXSize;
            m_blkBufYSize = bufYSize;
            m_blkBufInterleave = blkInterl;

            allocateBuffer();
        }

    public:
        ImgSpatialSubset& spatial() { return m_spatial; }
        void updateSpatial(int xOff, int yOff, int xSize, int ySize) {
            m_spatial.setRange(xOff, yOff, xSize, ySize);
        }

        ImgSpectralSubset& spectral() { return m_spectral; }

    public:
        int bufXSize() { return m_blkBufXSize; }
        int bufYSize() { return m_blkBufYSize; }

        // 参照 ENVI 的 Dims 意义
        int bufDims() {
            return m_blkBufXSize*m_blkBufYSize*m_spectral.count();
        }

        const ImgInterleaveType& interleave() const { return m_blkBufInterleave; }

    public:
        T* bufData() { return m_blkBufData; }

    private:
        void allocateBuffer() {
            freeBuffer();
            m_blkBufData = new T[bufDims()]{};
        }
        void freeBuffer() {
            if (m_blkBufData) {
                delete[](m_blkBufData);
                m_blkBufData = nullptr;
            }
        }

    private:
        ImgSpatialSubset m_spatial;     // 处理每一块的时候都需要更新
        ImgSpectralSubset m_spectral;   // 一般只需初始化一次

    private:
        ImgInterleaveType m_blkBufInterleave;
        int m_blkBufXSize;
        int m_blkBufYSize;

    private:
        T *m_blkBufData;
    };

    // 仿函数，用于读取分块数据
    template <typename T>
    class ImgBlockDataRead {
    public:
        ImgBlockDataRead(GDALDataset *dataset) {
            // todo 能否在构造函数中抛出异常 ???
            if (!dataset)
                throw std::runtime_error("GDALDataset is nullptr.");

            imgDataset_ = dataset;
            imgDT_ = toGDALDataType<T>();
        }

        bool operator() (ImgBlockData<T> &data) {
            int xOff = data.spatial().xOff();
            int yOff =data.spatial().yOff();
            int xSize = data.spatial().xSize();
            int ySize = data.spatial().ySize();
            int count = data.spectral().count();
            T *buffer = data.bufData();

            switch (data.interleave()) {
                case ImgInterleaveType::IIT_BIP :
                {
                    // todo 将全波段处理和部分波段处理分开
                    if ( CPLErr::CE_Failure == imgDataset_->RasterIO(GF_Read,
                            xOff, yOff, xSize, ySize, buffer, xSize, ySize,
                            imgDT_, count, data.spectral().map(),
                            sizeof(T)*count,
                            sizeof(T)*count*xSize,
                            sizeof(T)) ) {
                        return false;
                    }
                    break;
                }

                case ImgInterleaveType::IIT_BSQ :
                {
                    if ( CPLErr::CE_Failure == imgDataset_->RasterIO(GF_Read,
                            xOff, yOff, xSize, ySize, buffer, xSize, ySize,
                            imgDT_, count, data.spectral().map(),
                            0, 0, 0) ) {
                        return false;
                    }
                    break;
                }

                case ImgInterleaveType::IIT_BIL :
                {
                    if ( CPLErr::CE_Failure == imgDataset_->RasterIO(GF_Read,
                            xOff, yOff, xSize, ySize, buffer, xSize, ySize,
                            imgDT_, count, data.spectral().map(),
                            sizeof(T),
                            sizeof(T)*count*xSize,
                            sizeof(T)*xSize) ) {
                        return false;
                    }
                    break;
                }

            }// end switch
        }

    private:
        GDALDataset *imgDataset_;
        GDALDataType imgDT_;
    };


} // namespace ImgTool

#endif //IMG_PROCESS_IMGTOOL_COMMON_HPP
