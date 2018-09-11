//
// Created by penglei on 18-9-8.
//

#ifndef IMG_PROCESS_IMGTOOL_IMGBLOCKPROCESS_HPP
#define IMG_PROCESS_IMGTOOL_IMGBLOCKPROCESS_HPP

#include "imgtool_common.hpp"
#include <functional>

namespace ImgTool {

    template <typename T>
    class ImgBlockProcess {
    public:
        // 用于“全波段”处理
        ImgBlockProcess(GDALDataset *dataset,
                int blockSize = 8,
                ImgBlockType blockType = ImgBlockType::IBT_LINE,
                ImgInterleave dataInterleave = ImgInterleave::BIP);

        // 用于“指定波段”的处理
        ImgBlockProcess(GDALDataset *dataset,
                int proBandCount, int *proBandMap,
                int blockSize = 8,
                ImgBlockType blockType = ImgBlockType::IBT_LINE,
                ImgInterleave dataInterleave = ImgInterleave::BIP);

        ImgBlockProcess(const ImgBlockProcess &other)
            : m_blkData(other.m_blkData), m_spectralSubset(other.m_spectralSubset) {
                m_poDataset = other.m_poDataset;
                m_imgXSize = other.m_imgXSize;
                m_imgYSize = other.m_imgYSize;
                m_imgBandCount = other.m_imgBandCount;

                m_blkType = other.m_blkType;
                m_dataInterleave = other.m_dataInterleave;
        }

        ImgBlockProcess& operator= (const ImgBlockProcess &other) {
            m_poDataset = other.m_poDataset;
            m_imgXSize = other.m_imgXSize;
            m_imgYSize = other.m_imgYSize;
            m_imgBandCount = other.m_imgBandCount;
            m_spectralSubset = other.m_spectralSubset;

            m_blkType = other.m_blkType;
            m_dataInterleave = other.m_dataInterleave;
            m_blkData = other.m_blkData;
        }

        virtual ~ImgBlockProcess() {
        }

    public:

        template <class Fn, class... Args>
        void processBlockData(Fn &&fn, Args &&... args);

        ImgBlockData<T>& data() { return m_blkData; }

    public:
        int bufXSize() { return m_blkData.bufXSize(); }
        int bufYSize() { return m_blkData.bufYSize(); }
        int spectralCount() { return m_spectralSubset.count(); }

        int bufDims() { return m_blkData.bufDims(); }

    private:
        void initImgInfo() {
            // todo 是否必须 ？？？
            m_imgBandCount = m_poDataset->GetRasterCount();
            m_imgXSize = m_poDataset->GetRasterXSize();
            m_imgYSize = m_poDataset->GetRasterYSize();
        }

        void initProcessBands(int proBandCount, int *proBandMap = nullptr) {
            for (int i = 0; i < proBandCount; i++) {
                proBandMap ? m_spectralSubset.addSpectral(proBandMap[i]) :
                m_spectralSubset.addSpectral(i+1);
            }
        }

        void initBlockData() {
            m_blkData.blkSpectralSubset(m_spectralSubset);
            m_blkData.dataInterleave(m_dataInterleave);

            if (m_blkType == ImgBlockType::IBT_LINE) {
                m_blkData.setBufSize(m_imgXSize, m_blkSize);
            } else {
                m_blkData.setBufSize(m_blkSize, m_blkSize);
            }

            m_blkData.allocateBuffer();
        }

    protected:
        GDALDataset *m_poDataset;
        int m_imgBandCount;
        int m_imgXSize;
        int m_imgYSize;

    private:
        int m_blkSize;
        ImgBlockType m_blkType;
        ImgInterleaveType m_dataInterleave;
        ImgSpectralSubset m_spectralSubset;

    private:
        ImgBlockData<T> m_blkData;
    };

    template <typename T>
    ImgBlockProcess<T>::ImgBlockProcess(GDALDataset *dataset,
            int blockSize/* = 8 */,
            ImgBlockType blockType/* = ImgBlockType::IBT_LINE */,
            ImgInterleave dataInterleave/* = ImgInterleave::BIP */) {

        m_poDataset = dataset;
        m_blkSize = blockSize;
        m_blkType = blockType;
        m_dataInterleave = dataInterleave;

        initImgInfo();
        initProcessBands(m_imgBandCount);
        initBlockData();
    }

    template <typename T>
    ImgBlockProcess<T>::ImgBlockProcess(GDALDataset *dataset,
            int proBandCount, int *proBandMap,
            int blockSize/* = 8 */,
            ImgBlockType blockType/* = ImgBlockType::IBT_LINE */,
            ImgInterleave dataInterleave/* = ImgInterleave::BIP */) {

        m_poDataset = dataset;
        m_blkSize = blockSize;
        m_blkType = blockType;
        m_dataInterleave = dataInterleave;

        initImgInfo();
        initProcessBands(proBandCount, proBandMap);
        initBlockData();
    }

    template <typename T>
    template <class Fn, class... Args>
    void ImgBlockProcess<T>::processBlockData(Fn &&fn, Args &&... args) {
        // 读取数据块的函数对象
        ImgBlockDataRead<T> funcRead(m_poDataset);

        // 数据块处理的核心函数
        auto funcBlockDataProcessCore = std::forward<Fn>(fn);

        switch (m_blkType) {
            case ImgBlockType::IBT_LINE :
            {
                int blockNums = m_imgYSize / m_blkSize;
                int leftLines = m_imgYSize % m_blkSize;
                m_blkData.spatial().xOff(0);
                m_blkData.spatial().xSize(m_imgXSize);
                m_blkData.spatial().ySize(m_blkSize);

                // 处理完整块
                for (int i = 0; i < blockNums; i++) {
                    m_blkData.spatial().yOff(i*m_blkSize);

                    // 从影像文件中读取指定的块数据
                    funcRead(m_blkData);

                    // todo<-------> 处理每一块数据，由使用者提供
                    funcBlockDataProcessCore(std::forward<Args>(args)...);
                }

                // 处理剩余的最后一块（非完整块）
                if (leftLines > 0) {
                    m_blkData.spatial().yOff(blockNums*m_blkSize);
                    m_blkData.spatial().ySize(leftLines);

                    funcRead(m_blkData);
                    funcBlockDataProcessCore(std::forward<Args>(args)...);
                }

                break;
            }

            case ImgBlockType::IBT_SQUARE :
            {
                for (int i = 0; i < m_imgYSize; i += m_blkSize) {
                    for (int j = 0; j < m_imgXSize; j += m_blkSize) {
                        int xBlockSize = m_blkSize;
                        int yBlockSize = m_blkSize;

                        // 如果最下面和最右面的块不够 m_blockSize, 剩下多少读多少
                        if (i + m_blkSize > m_imgYSize) // 最下面的剩余块
                            yBlockSize = m_imgYSize - i;
                        if (j + m_blkSize > m_imgXSize) // 最右侧的剩余块
                            xBlockSize = m_imgXSize - j;

                        m_blkData.updateSpatial(j, i, xBlockSize, yBlockSize);

                        funcRead(m_blkData);
                        funcBlockDataProcessCore(std::forward<Args>(args)...);
                    }
                }

                break;
            }
        }// end switch
    }


} // namespace ImgTool


#endif //IMG_PROCESS_IMGTOOL_IMGBLOCKPROCESS_HPP
