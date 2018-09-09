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
            : m_blockData(other.m_blockData) {
                m_poDataset = other.m_poDataset;
                m_imgXSize = other.m_imgXSize;
                m_imgYSize = other.m_imgYSize;
                m_imgBandCount = other.m_imgBandCount;

                m_proBandCount = other.m_proBandCount;
                m_proBandMap = new int[m_proBandCount];
                memcpy(m_proBandMap, other.m_proBandMap, sizeof(int)*m_proBandCount);

                m_blockType = other.m_blockType;
                m_dataInterleave = other.m_dataInterleave;
        }

        ImgBlockProcess& operator= (const ImgBlockProcess &other) {
            m_poDataset = other.m_poDataset;
            m_imgXSize = other.m_imgXSize;
            m_imgYSize = other.m_imgYSize;
            m_imgBandCount = other.m_imgBandCount;

            m_proBandCount = other.m_proBandCount;

            if (m_proBandMap) {
                delete[](m_proBandMap);
                m_proBandMap = nullptr;
            }
            m_proBandMap = new int[m_proBandCount];
            memcpy(m_proBandMap, other.m_proBandMap, sizeof(int)*m_proBandCount);

            m_blockType = other.m_blockType;
            m_dataInterleave = other.m_dataInterleave;
            m_blockData = other.m_blockData;
        }

        virtual ~ImgBlockProcess() {
            if (m_proBandMap) {
                delete[](m_proBandMap);
                m_proBandMap = nullptr;
            }
        }

    public:

        template <class Fn, class... Args>
        void processBlockData(Fn &&fn, Args &&... args);

        ImgBlockData<T>& data() { return m_blockData; }

    public:
        int bufXSize() { return m_blockData.bufXSize(); }
        int bufYSize() { return m_blockData.bufYSize(); }
        int proBandCount() const { return m_proBandCount; }

        int blockDataBufSize() { return m_blockData.bufSize(); }

    private:
        void initImgInfo() {
            // todo 是否必须 ？？？
            m_imgBandCount = m_poDataset->GetRasterCount();
            m_imgXSize = m_poDataset->GetRasterXSize();
            m_imgYSize = m_poDataset->GetRasterYSize();
        }

        void initProcessBands(int proBandCount, int *proBandMap = nullptr) {
            m_proBandCount = proBandCount;
            m_proBandMap = new int[m_proBandCount];

            for (int i = 0; i < m_proBandCount; i++) {
                proBandMap ? m_proBandMap[i] = proBandMap[i] : m_proBandMap[i] = i + 1;
            }
        }

        void initBlockData() {
            m_blockData.setProcessBands(m_proBandCount, m_proBandMap);
            m_blockData.dataInterleave(m_dataInterleave);

            if (m_blockType == ImgBlockType::IBT_LINE) {
                m_blockData.setBufSize(m_imgXSize, m_blockSize);
            } else {
                m_blockData.setBufSize(m_blockSize, m_blockSize);
            }

            m_blockData.allocateBuffer();
        }

    protected:
        GDALDataset *m_poDataset;
        int m_imgBandCount;
        int m_imgXSize;
        int m_imgYSize;

    private:
        int m_proBandCount;
        int *m_proBandMap;

        int m_blockSize;
        ImgBlockType m_blockType;
        ImgInterleave m_dataInterleave;

    private:
        ImgBlockData<T> m_blockData;
    };

    template <typename T>
    ImgBlockProcess<T>::ImgBlockProcess(GDALDataset *dataset,
            int blockSize/* = 8 */,
            ImgBlockType blockType/* = ImgBlockType::IBT_LINE */,
            ImgInterleave dataInterleave/* = ImgInterleave::BIP */) {

        m_poDataset = dataset;
        m_blockSize = blockSize;
        m_blockType = blockType;
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
        m_blockSize = blockSize;
        m_blockType = blockType;
        m_dataInterleave = dataInterleave;

        initImgInfo();
        initProcessBands(proBandCount, proBandMap);
        initBlockData();
    }

    template <typename T>
    template <class Fn, class... Args>
    void ImgBlockProcess<T>::processBlockData(Fn &&fn, Args &&... args) {
        // 读取数据块的函数对象
        ImgBlockDataRead<T> funcRead(m_poDataset, m_proBandCount, m_proBandMap);

        // 数据块处理的核心函数
        auto funcBlockDataProcessCore = std::forward<Fn>(fn);

        switch (m_blockType) {
            case ImgBlockType::IBT_LINE:
            {
                int blockNums = m_imgYSize / m_blockSize;
                int leftLines = m_imgYSize % m_blockSize;
                m_blockData.xOff(0);
                m_blockData.xSize(m_imgXSize);
                m_blockData.ySize(m_blockSize);

                // 处理完整块
                for (int i = 0; i < blockNums; i++) {
                    m_blockData.yOff(i*m_blockSize);

                    // 从影像文件中读取指定的块数据
                    funcRead(m_blockData);

                    // todo<-------> 处理每一块数据，由使用者提供
                    //funcBlockDataProcessCore(m_blockData, std::forward<Args>(args)...);
                    funcBlockDataProcessCore(std::forward<Args>(args)...);
                }

                // 处理剩余的最后一块（非完整块）
                if (leftLines > 0) {
                    m_blockData.yOff(blockNums*m_blockSize);
                    m_blockData.ySize(leftLines);

                    funcRead(m_blockData);
                    //funcBlockDataProcessCore(m_blockData, std::forward<Args>(args)...);
                    funcBlockDataProcessCore(std::forward<Args>(args)...);
                }

                break;
            }

            case ImgBlockType::IBT_SQUARE:
            {
                for (int i = 0; i < m_imgYSize; i += m_blockSize) {
                    for (int j = 0; j < m_imgXSize; j += m_blockSize) {
                        int xBlockSize = m_blockSize;
                        int yBlockSize = m_blockSize;

                        // 如果最下面和最右面的块不够 m_blockSize, 剩下多少读多少
                        if (i + m_blockSize > m_imgYSize) // 最下面的剩余块
                            yBlockSize = m_imgYSize - i;
                        if (j + m_blockSize > m_imgXSize) // 最右侧的剩余块
                            xBlockSize = m_imgXSize - j;

                        m_blockData.xOff(j);
                        m_blockData.yOff(i);
                        m_blockData.xSize(xBlockSize);
                        m_blockData.ySize(yBlockSize);

                        funcRead(m_blockData);

                        //funcBlockDataProcessCore(m_blockData, std::forward<Args>(args)...);
                        funcBlockDataProcessCore(std::forward<Args>(args)...);
                    }

                }

                break;
            }
        }// end switch
    }


} // namespace ImgTool


#endif //IMG_PROCESS_IMGTOOL_IMGBLOCKPROCESS_HPP
