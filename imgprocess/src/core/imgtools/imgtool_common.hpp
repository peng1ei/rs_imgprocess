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
#include <chrono>
#include <functional>
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

        void updateEnvelope(int xOff, int yOff, int xSize, int ySize) {
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

    struct ImgBlockDims {
        int xSize;
        int ySize;
        int bandCount;
    };

    // 光谱处理子集
    // 用 std::vector<int> 实现，存储待处理波段的索引号（从 1 开始）
    // vector 的大小表示需处理的波段数
    struct ImgSpectralSubset {
        int spectralCount() { return static_cast<int>(spectral.size()); }
        int* spectralMap() { return spectral.data(); }

        std::pair<int, int*> spectralSubset() {
            std::pair<int, int*> tmp(static_cast<int>(spectral.size()), spectral.data());
            return tmp;
        }

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
        ImgBlockData()
            : m_blkSpectral(), m_blkBufInterleave(ImgInterleave::BIP),
            m_blkEnvelope(), m_blkBufData(nullptr) {
            m_blkBufXSize = 0;
            m_blkBufYSize = 0;
        }

        ImgBlockData(int bufXSize, int bufYSize, int proBandCount, int *proBandMap)
            : m_blkBufInterleave(ImgInterleave::BIP),
            m_blkEnvelope(), m_blkBufData(nullptr){
            m_blkBufXSize = bufXSize;
            m_blkBufYSize = bufYSize;

            for (int i = 0; i < proBandCount; i++) {
                m_blkSpectral.addSpectral(proBandMap[i]);
            }

            allocateBuffer();
        }

        ImgBlockData(int bufXSize, int bufYSize, ImgSpectralSubset &specSubset)
            : m_blkSpectral(specSubset), m_blkBufInterleave(ImgInterleave::BIP),
            m_blkEnvelope(), m_blkBufData(nullptr) {
            m_blkBufXSize = bufXSize;
            m_blkBufYSize = bufYSize;

            allocateBuffer();
        }

        virtual ~ImgBlockData() { freeBuffer(); }

        // todo 移动构造/赋值函数
        ImgBlockData(const ImgBlockData &other)
            : m_blkSpectral(other.m_blkSpectral),
            m_blkBufInterleave(other.m_blkBufInterleave),
            m_blkEnvelope(other.m_blkEnvelope),
            m_blkBufData(nullptr) {
            m_blkBufXSize = other.m_blkBufXSize;
            m_blkBufYSize = other.m_blkBufYSize;

            allocateBuffer();
            memcpy(m_blkBufData, other.m_blkBufData, sizeof(T)*bufDims());

            std::cout << "copy ctor" << std::endl;
        }

        ImgBlockData& operator= (const ImgBlockData &other) {
            if (this == &other)
                return *this;

            m_blkEnvelope = other.m_blkEnvelope;
            m_blkBufInterleave = other.m_blkBufInterleave;
            m_blkSpectral = other.m_blkSpectral;
            m_blkBufXSize = other.m_blkBufXSize;
            m_blkBufYSize = other.m_blkBufYSize;
            m_blkBufData = nullptr;

            allocateBuffer();
            memcpy(m_blkBufData, other.m_blkBufData, sizeof(T)*bufDims());

            std::cout << "assignment ctor" << std::endl;
            return *this;
        }

    public:
        ImgBlockEnvelope& blkEnvelope() { return m_blkEnvelope; }
        void updateBlkEnvelope(int xOff, int yOff, int xSize, int ySize) {
            m_blkEnvelope.updateEnvelope(xOff, yOff, xSize, ySize);
        }

        ImgSpectralSubset& blkSpectralSubset() { return m_blkSpectral; }
        void blkSpectralSubset(ImgSpectralSubset &spec) {
            m_blkSpectral = spec;
        }

    public:
        const ImgInterleave& dataInterleave() const { return m_blkBufInterleave; }
        void dataInterleave(ImgInterleave &interleave) { m_blkBufInterleave = interleave; }

    public:
        int bufXSize() const { return m_blkBufXSize; }
        int bufYSize() const { return m_blkBufYSize; }
        void setBufSize(int bufXSize, int bufYSize) {
            m_blkBufXSize = bufXSize;
            m_blkBufYSize = bufYSize;
        }

        // 参照 ENVI 的 Dims 意义
        int bufDims() {
            return m_blkBufXSize*m_blkBufYSize*m_blkSpectral.spectralCount();
        }

    public:
        T* bufData() { return m_blkBufData; }

        void allocateBuffer() {
            freeBuffer();
            m_blkBufData = new T[bufDims()]{};
        }

        //void allocateBuffer(int bufXSize, int bufYSize, ) {
        //    freeBuffer();
        //    m_blkBufData = new T[bufDims()]{};
        //}

    private:
        void freeBuffer() {
            if (m_blkBufData) {
                delete[](m_blkBufData);
                m_blkBufData = nullptr;
            }
        }

    private:
        // 处理每一块的时候都需要更新
        ImgBlockEnvelope m_blkEnvelope;

    private:
        ImgInterleave m_blkBufInterleave;
        ImgSpectralSubset m_blkSpectral;

    private:
        int m_blkBufXSize;
        int m_blkBufYSize;
        T *m_blkBufData;
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
            m_dt = toGDALDataType<T>();
            m_proBandCount = proBandCount;
            m_proBandMap = proBandMap;
        }

        ImgBlockDataRead(GDALDataset *dataset, ImgSpectralSubset &specSubset) {
            // todo 能否在构造函数中抛出异常 ???
            if (!dataset)
                throw std::runtime_error("GDALDataset is nullptr.");

            m_poDataset = dataset;
            m_dt = toGDALDataType<T>();
            m_proBandCount = specSubset.spectralCount();
            m_proBandMap = specSubset.spectralMap();
        }

        bool operator() (ImgBlockData<T> &data) {
            int xOff = data.blkEnvelope().xOff();
            int yOff =data.blkEnvelope().yOff();
            int xSize = data.blkEnvelope().xSize();
            int ySize = data.blkEnvelope().ySize();
            T *buffer = data.bufData();

            switch (data.dataInterleave()) {
                case ImgInterleave::BIP :
                {
                    // todo 将全波段处理和部分波段处理分开
                    if ( CPLErr::CE_Failure == m_poDataset->RasterIO(GF_Read,
                            xOff, yOff, xSize, ySize, buffer, xSize, ySize,
                            m_dt, m_proBandCount, m_proBandMap,
                            sizeof(T)*m_proBandCount,
                            sizeof(T)*m_proBandCount*xSize,
                            sizeof(T)) ) {
                        return false;
                    }
                    break;
                }

                case ImgInterleave::BSQ :
                {
                    if ( CPLErr::CE_Failure == m_poDataset->RasterIO(GF_Read,
                            xOff, yOff, xSize, ySize, buffer, xSize, ySize,
                            m_dt, m_proBandCount, m_proBandMap, 0, 0, 0) ) {
                        return false;
                    }
                    break;
                }

                case ImgInterleave::BIL :
                {
                    if ( CPLErr::CE_Failure == m_poDataset->RasterIO(GF_Read,
                            xOff, yOff, xSize, ySize, buffer, xSize, ySize,
                            m_dt, m_proBandCount, m_proBandMap,
                            sizeof(T),
                            sizeof(T)*m_proBandCount*xSize,
                            sizeof(T)*xSize) ) {
                        return false;
                    }
                    break;
                }

            }// end switch
        }

    private:
        GDALDataset *m_poDataset;
        GDALDataType m_dt;
        int m_proBandCount;
        int *m_proBandMap;
    };

    // 用于测试算法时间


    // 精度修改
    // milliseconds : 毫秒
    // microseconds : 微秒
    // nanoseconds : 纳秒
    template<typename TimeT = std::chrono::milliseconds>
    struct measure
    {
        template<typename F, typename ...Args>
        static typename TimeT::rep execution(F &&fn, Args&&... args)
        {
            auto start = std::chrono::system_clock::now();

            // Now call the function with all the parameters you need.
            std::bind(std::forward<F>(fn), std::forward<Args>(args)...)();
            //auto func = std::forward<F>(fn);
            //func(std::forward<Args>(args)...);

            auto duration = std::chrono::duration_cast<TimeT>
                    (std::chrono::system_clock::now() - start);

            return duration.count();
        }
    };

    // 使用
    /*
    struct functor
    {
        int state;
        functor(int state) : state(state) {}
        void operator()() const
        {
            std::cout << "In functor run for ";
        }
    };

    void func()
    {
        std::cout << "In function, run for " << std::endl;
    }

    int main()
    {
        // codes directly
        std::cout << measure<>::execution([&]() {
            // your code
        }) << " ms" << std::endl;

        // functor
        std::cout << measure<>::execution(functor(3)) << std::endl;

        // function
        std::cout << measure<>::execution(func);
    }
     */


} // namespace ImgTool

#endif //IMG_PROCESS_IMGTOOL_COMMON_HPP
