//
// Created by penglei on 18-10-10.
//

#ifndef IMGPROCESS_MG_COMMON_H
#define IMGPROCESS_MG_COMMON_H

// $1
// 处理的时候全部用double，保存出去的时候进行数据类型转换，进行相应的计算
#define MgSwitchGDALTypeProcess(outGdt, func) switch(outGdt) {\
    case GDALDataType::GDT_Byte:\
        return func<unsigned char>();\
    case GDALDataType::GDT_UInt16:\
        return func<unsigned short>();\
    case GDALDataType::GDT_Int16:\
        return func<short>();\
    case GDALDataType::GDT_UInt32:\
        return func<unsigned int>();\
    case GDALDataType::GDT_Int32:\
        return func<int>();\
    case GDALDataType::GDT_Float32:\
        return func<float>();\
    case GDALDataType::GDT_Float64:\
        return func<double>();\
    default:\
        return false;\
    }
// $1

#include <initializer_list>
#include <vector>
#include <thread>

namespace Mg {

    // 数据在内存中的组织方式
    enum class MgInterleave : char {
        BSQ,
        BIL,
        BIP
    };

    // 分块类型
    enum class MgBlockType : char {
        SQUARE,
        LINE
    };

    // 用于选择需要读取的波段
    struct MgBandMap {
        MgBandMap() {}
        MgBandMap(int count) // 全波段处理
            : bandMap_(static_cast<unsigned long>(count)) {
            int i = 0;
            for (auto &val : bandMap_) {
                val = ++i;
            }
        }
        MgBandMap(std::initializer_list<int> list) : bandMap_(list) {}

        int size() const { return static_cast<int>(bandMap_.size()); }

        int* data() { return bandMap_.data(); }
        const int* data() const { return bandMap_.data(); }

    private:
        std::vector<int> bandMap_;
    };

    /**
     * 根据硬件CPU核数,获取最佳线程数量
     * @param numTasks      总共的任务量
     * @param minPerThread  每个线程最小处理的任务量，默认为1
     * @return              返回最佳的线程数量
     */
    inline int getOptimalNumThreads(int numTasks, int minPerThread = 1) {
        int hardThreads = std::thread::hardware_concurrency();
        int maxThreads = (numTasks + minPerThread - 1) / minPerThread;
        return std::min(hardThreads != 0 ? hardThreads : 2, maxThreads);
    }

} // MatGdal

#endif //IMGPROCESS_MG_COMMON_H
