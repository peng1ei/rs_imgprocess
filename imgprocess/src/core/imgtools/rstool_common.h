//
// Created by penglei on 18-9-21.
//

#ifndef IMGPROCESS_RSTOOL_COMMON_H
#define IMGPROCESS_RSTOOL_COMMON_H

#include <vector>

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

        int elemCount() const { return xSize_*ySize_*bands_.size();}
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

        DataChunk(DataChunk &&rother) noexcept
            : dims_(rother.dims_), intl_(rother.intl_) {

            // 偷取
            data_ = rother.data_;
            rother.data_ = nullptr;
        }

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
            dims_.xOff_ = xOff;
            dims_.yOff_ = yOff;
            dims_.xSize_ = xSize;
            dims_.ySize_ = ySize;
            mempcpy(data_, data, sizeof(T)*dims_.elemCount());
        }

        void replace(int xOff, int yOff, int xSize, int ySize, T *data) {
            ReleaseArray(data_);
            dims_.xOff_ = xOff;
            dims_.yOff_ = yOff;
            dims_.xSize_ = xSize;
            dims_.ySize_ = ySize;
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



} // namespace RSTool

#endif //IMGPROCESS_RSTOOL_COMMON_H
