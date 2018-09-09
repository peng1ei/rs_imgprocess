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
#include "gdal/gdal.h"
#include "gdal/gdal_priv.h"

class GDALDataset;

namespace ImgTool {

    enum ImgInterleave {
        BSQ,
        BIL,
        BIP
    };

    enum ImgBlockType {
        IBT_LINE,   /* 以行为基本单位进行分块 */
        IBT_SQUARE  /* 以方形进行分块 */
    };

    // 用于将内置类型转换为 GDALDataType
    template <typename T>
    GDALDataType toGDALDataType() {
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

    //struct ImgDims {
    //int xSize;
    //        int ySize;
    //        int bandCount;
    //    };

    // 用于块处理时指定块空间范围
    struct ImgBlockEnvelope {
        ImgBlockEnvelope() : xOff_(0), yOff_(0), xSize_(0), ySize_(0) {}
        ImgBlockEnvelope(int xOff, int yOff, int xSize, int ySize) {
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

    private:
        int xOff_;   // 起始偏移量从 0 开始
        int yOff_;   // 起始偏移量从 0 开始
        int xSize_;
        int ySize_;
    };

    // 光谱处理子集
    // 用 std::vector<int> 实现，存储待处理波段的索引号（从 1 开始）
    // vector 的大小表示需处理的波段数
    struct ImgSpectralSubset {
        int spectralCount() { return static_cast<int>(spectral.size()); }
        int* spectralMap() { return spectral.data(); }
        void addSpectral(int index) {
            spectral.push_back(index);
        };

    private:
        std::vector<int> spectral;
    };

    // 空间处理范围
    struct ImgSpatialSubset {
        ImgSpatialSubset() : spatial() {}
        ImgSpatialSubset(int xOff, int yOff, int xSize, int ySize)
            : spatial(xOff, yOff, xSize, ySize) {}

        int xOff() const { return spatial.xOff(); }
        void xOff(int value) { spatial.xOff(value); }

        int yOff() const { return spatial.yOff(); }
        void yOff(int value) { spatial.yOff(value); }

        int xSize() const { return spatial.xSize(); }
        void xSize(int value) { spatial.xSize(value); }

        int ySize() const { return spatial.ySize(); }
        void ySize(int value) { spatial.ySize(value); }

    private:
        ImgBlockEnvelope spatial;
    };

    template <typename T>
    struct ImgBlockData {
        ImgBlockData() : m_bufData(nullptr), m_proBandMap(nullptr) {}
        ImgBlockData(int bufXSize, int bufYSize, int proBandCount, int *proBandMap) {
            m_bufXSize = bufXSize;
            m_bufYSize = bufYSize;
            m_proBandCount = proBandCount;
            m_proBandMap = nullptr;
            m_bufData = nullptr;

            allocateBuffer();
        }
        virtual ~ImgBlockData() { release(); }

        // todo 移动构造/赋值函数
        ImgBlockData(const ImgBlockData &other) {
            this->m_proBandCount = other.m_proBandCount;
            this->m_proBandMap = other.m_proBandMap;
            this->m_xOff = other.m_xOff;
            this->m_yOff = other.m_yOff;
            this->m_xSize = other.m_xSize;
            this->m_ySize = other.m_ySize;
            this->m_dataInterleave = other.m_dataInterleave;
            this->m_bufXSize = other.m_bufXSize;
            this->m_bufYSize = other.m_bufYSize;
            m_bufData = nullptr;

            allocateBuffer();
            memcpy(this->m_bufData, other.m_bufData, sizeof(T)*bufSize());

            std::cout << "copy ctor" << std::endl;
        }

        ImgBlockData& operator= (const ImgBlockData &other) {
            if (this == &other)
                return *this;

            this->m_proBandCount = other.m_proBandCount;
            this->m_proBandMap = other.m_proBandMap;
            this->m_xOff = other.m_xOff;
            this->m_yOff = other.m_yOff;
            this->m_xSize = other.m_xSize;
            this->m_ySize = other.m_ySize;
            this->m_dataInterleave = other.m_dataInterleave;
            this->m_bufXSize = other.m_bufXSize;
            this->m_bufYSize = other.m_bufYSize;

            allocateBuffer();
            memcpy(this->m_bufData, other.m_bufData, sizeof(T)*bufSize());

            std::cout << "assigment ctor" << std::endl;

            return *this;
        }


    public:
        int xOff() const { return m_xOff; }
        void xOff(int value) { m_xOff = value; }

        int yOff() const { return m_yOff; }
        void yOff(int value) { m_yOff = value; }

        int xSize() const { return m_xSize; }
        void xSize(int value) { m_xSize = value; }

        int ySize() const { return m_ySize; }
        void ySize(int value) { m_ySize = value; }

        void getProcessBands(int &bandCount, int *bandMap = nullptr) const {
            bandCount = m_proBandCount;
            if (bandMap) bandMap = m_proBandMap;
        }

        void setProcessBands(int bandCount, int bandMap[]) {
            m_proBandCount = bandCount;
            m_proBandMap = bandMap;
        }

        const ImgInterleave& dataInterleave() const { return m_dataInterleave; }
        void dataInterleave(ImgInterleave &interleave) { m_dataInterleave = interleave; }

    public:
        int bufXSize() const { return m_bufXSize; }
        int bufYSize() const { return m_bufYSize; }
        void setBufSize(int bufXSize, int bufYSize) {
            m_bufXSize = bufXSize;
            m_bufYSize = bufYSize;
        }

        int bufSize() const { return m_bufXSize*m_bufYSize*m_proBandCount; }

    public:
        T* bufData() { return m_bufData; }

        void allocateBuffer() {
            release();
            m_bufData = new T[bufSize()]{};
        }

        void allocateBuffer(int bufXSize, int bufYSize, int proBandCount) {
            release();
            m_bufData = new T[bufSize()]{};
        }

    private:
        void release() {
            if (m_bufData) {
                delete[](m_bufData);
                m_bufData = nullptr;
            }
        }

    private:
        // 处理每一块的时候都需要更新
        int m_xOff;
        int m_yOff;
        int m_xSize;
        int m_ySize;
        //ImgBlockEnvelope m_blkEnvelope;

    private:
        int m_proBandCount;
        int *m_proBandMap;
        ImgInterleave m_dataInterleave;
        std::pair<int, int*> m_proBands;

    private:
        int m_bufXSize;
        int m_bufYSize;
        T *m_bufData;
    };

    // 仿函数，用于读取分块数据
    template <typename T>
    class ImgBlockDataRead {
    public:
        ImgBlockDataRead(GDALDataset *dataset, int proBandCount, int *proBandMap) {
            // todo 能否在构造函数中抛出异常 ???
            if (!dataset)
                throw std::runtime_error("GDALDataset is nullptr.");

            m_poDataset = dataset;
            m_proBandCount = proBandCount;
            m_proBandMap = proBandMap;

            m_imgXSize = m_poDataset->GetRasterXSize();
            m_dt = toGDALDataType<T>();
        }

        bool operator() (ImgBlockData<T> &data) {
            int xOff = data.xOff();
            int yOff = data.yOff();
            int xSize = data.xSize();
            int ySize = data.ySize();
            T *buffer = data.bufData();

            switch (data.dataInterleave()) {
                case ImgInterleave::BIP:
                {
                    // todo 将全波段处理和部分波段处理分开
                    if ( CPLErr::CE_Failure == m_poDataset->RasterIO(GF_Read,
                            xOff, yOff, xSize, ySize, buffer, xSize, ySize,
                            m_dt, m_proBandCount, m_proBandMap,
                            sizeof(T)*m_proBandCount,
                            sizeof(T)*m_proBandCount*m_imgXSize,
                            sizeof(T)) ) {
                        return false;
                    }
                    break;
                }

                case ImgInterleave::BSQ:
                {
                    if ( CPLErr::CE_Failure == m_poDataset->RasterIO(GF_Read,
                            xOff, yOff, xSize, ySize, buffer, xSize, ySize,
                            m_dt, m_proBandCount, m_proBandMap, 0, 0, 0) ) {
                        return false;
                    }
                    break;
                }

                case ImgInterleave::BIL:
                {
                    if ( CPLErr::CE_Failure == m_poDataset->RasterIO(GF_Read,
                            xOff, yOff, xSize, ySize, buffer, xSize, ySize,
                            m_dt, m_proBandCount, m_proBandMap,
                            sizeof(T),
                            sizeof(T)*m_proBandCount*m_imgXSize,
                            sizeof(T)*m_imgXSize) ) {
                        return false;
                    }
                    break;
                }

            }// end switch
        }

    private:
        GDALDataset *m_poDataset;
        GDALDataType m_dt;
        int m_imgXSize;
        int m_proBandCount;
        int *m_proBandMap;
    };

} // namespace ImgTool

#endif //IMG_PROCESS_IMGTOOL_COMMON_HPP
