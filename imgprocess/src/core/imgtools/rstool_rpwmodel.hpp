//
// Created by penglei on 18-9-22.
//
// 用于 “读-处理-写”模型算法（rpw-model）

#ifndef IMGPROCESS_RSTOOL_RPWMODEL_HPP
#define IMGPROCESS_RSTOOL_RPWMODEL_HPP

#include "rstool_rpmodel.hpp"

namespace RSTool {

    namespace Mp {

        template <typename InDataType, typename OutDataType>
        class MpRPWModel : public MpRPModel<InDataType> {
        public:
            /**
             *
             * @param infile                输入文件
             * @param outfile               输出文件
             * @param inSpecDims            输入文件的光谱范围
             * @param inIntl                输入文件数据在内存的组织方式
             * @param blkSize               指定处理的块大小
             * @param readThreadsCount      读线程数，默认为 1
             * @param writeThreadsCount     写线程数，默认为 1
             */
            MpRPWModel(const std::string &infile, const std::string &outfile,
                       const SpectralDimes &inSpecDims, Interleave inIntl = Interleave::BIP,
                       int blkSize = 128, int readThreadsCount = 1, int writeThreadsCount = 1)

                    : MpRPModel<InDataType>(infile, inSpecDims, inIntl, blkSize, readThreadsCount),
                    outfile_(outfile), mpWrite_(outfile, writeThreadsCount) {

                // TODO 确定读写缓冲区队列大小
                mpWrite_.writeQueueMaxSize_ = 2*MpRPModel<InDataType>::consumerCount_;
            }

            void setWriteQueueMaxSize(int value) { mpWrite_.writeQueueMaxSize_ = value; }

            // 支持多线程写数据
            void writeDataChunk(DataChunk<OutDataType> &&data) {
                {
                    // 等待队列中有空闲位置
                    std::unique_lock<std::mutex> lk(mpWrite_.mutexWriteQueue_);
                    while (mpWrite_.writeQueue_.size() == mpWrite_.writeQueueMaxSize_
                        /*&& !MpGDALWrite<T>::stop*/) {
                        mpWrite_.condWriteQueueNotFull_.wait(lk);
                    }

                    /*if (MpGDALWrite<T>::stop) return;*/

                    // 将准备输出的块数据移动到写缓冲队列中
                    mpWrite_.writeQueue_.emplace(std::move(data));
                }
                mpWrite_.condWriteQueueNotEmpty_.notify_all();
            }

            void run() {
                MpRPModel<InDataType>::run();

                {
                    // 停止写线程
                    std::unique_lock<std::mutex> lk(mpWrite_.mutexWriteQueue_);
                    mpWrite_.stop = true;
                }
                mpWrite_.condWriteQueueNotEmpty_.notify_all();
            }

        private:
            std::string outfile_;
            MpGDALWrite<OutDataType> mpWrite_;
        };

    } // namespace Mp

} // namespace RSTool

#endif //IMGPROCESS_RSTOOL_RPWMODEL_HPP
