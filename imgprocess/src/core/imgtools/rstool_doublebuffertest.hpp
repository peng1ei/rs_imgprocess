//
// Created by penglei on 18-9-24.
//

#ifndef IMGPROCESS_RSTOOL_DOUBLEBUFFERTEST_HPP
#define IMGPROCESS_RSTOOL_DOUBLEBUFFERTEST_HPP

#include "rstool_threadpool.h"

namespace RSTool {

    namespace Mp {

        template <typename T>
        class MpGDALReadT {
        public:
            static std::queue<DataChunk<T>> readQueue_; // 用于缓存从磁盘读取的数据
            static std::queue<DataChunk<T>> writeQueue_; // 用于缓存从磁盘读取的数据
            static int readQueueMaxSize_;
            static std::mutex mutexReadQueue_;
            static std::mutex mutexWriteQueue_;
            static std::condition_variable condReadQueueNotEmpty_;
            static std::condition_variable condReadQueueNotFull_;

        public:
            // 一般来说，进行读文件时，需要读的波段范围和数据在内存中的组织方式是已知的，
            // 如果采用分块读取，后续更新的只是数据块的空间范围，波段范围只需初始化一次即可

            /**
             *
             * @param infile        输入文件
             * @param specDims      指定光谱范围
             * @param intl          指定数据在内存中的组织方式
             * @param blkSize       指定分块大小
             * @param poolsCount    指定需要并行读取的线程数量
             */
            MpGDALReadT(const std::string &infile, const SpectralDimes &specDims,
                       Interleave &intl = Interleave::BIP, int poolsCount = 4)
                    : infile_(infile), specDims_(specDims), intl_(intl),
                      poolsCount_(poolsCount), pools_(poolsCount_), datasets_(poolsCount_) {

                for (auto &ds : datasets_) {
                    ds = (GDALDataset*)GDALOpen(infile.c_str(), GA_ReadOnly);
                }
            }

            ~MpGDALReadT() {
                for (auto &ds : datasets_) {
                    GDALClose((GDALDatasetH)ds);
                }
            }

            /**
             * 给每个读线程添加任务
             * @param i         第 i 个读线程，索引从 0 开始
             * @param spatDims  数据块的空间范围
             */
            void enqueue(int i, const SpatialDims &spatDims) {
                GDALDataset *ds = datasets_[i];
                pools_[i].enqueue([this, ds, spatDims] {

                    DataChunk<T> data(spatDims, specDims_, intl_);
                    ReadDataChunk<T> read(ds, specDims_, intl_);
                    if ( !read(spatDims.xOff(), spatDims.yOff(),
                               spatDims.xSize(), spatDims.ySize(), data.data())) {
                        throw std::runtime_error("Reading data chunk is faild.");
                    }

//                    {
//                        // 等待队列中有空闲位置
//                        std::unique_lock<std::mutex> lk(mutexReadQueue_);
//                        while (readQueue_.size() == readQueueMaxSize_) {
//                            condReadQueueNotFull_.wait(lk);
//                        }
//
//                        readQueue_.emplace(std::move(data));
//                    }
//                    condReadQueueNotEmpty_.notify_all();

                    while (true) {

                        {
                            // 等待队列中有空闲位置
                            std::unique_lock<std::mutex> lk(mutexWriteQueue_);
                            if (writeQueue_.size() < readQueueMaxSize_) {
                                writeQueue_.emplace(std::move(data));

                                if (writeQueue_.size() == 1/**readQueueMaxSize_**/) {
                                    // TODO 先解锁通知交换线程进行缓冲区交换
                                    lk.unlock();
                                    condReadQueueNotEmpty_.notify_all(); // 通知等待的线程，写缓冲区已非空
                                }
                                return;
                            }

                            // TODO 缓冲区已满，等待为空
                            while (writeQueue_.size() == readQueueMaxSize_) {
                                condReadQueueNotFull_.wait(lk);
                            }
                        }
                    } // end while

                }); // end lambad
            } // end enqueue()

        private:
            std::string infile_;
            SpectralDimes specDims_;
            Interleave intl_;

        private:
            int poolsCount_; // 读线程 数量
            std::vector<ThreadPool> pools_;
            std::vector<GDALDataset*> datasets_;
        };

        // $1
        template <typename T>
        std::queue<DataChunk<T>> MpGDALReadT<T>::readQueue_; // 用于缓存从磁盘读取的数据

        template <typename T>
        std::queue<DataChunk<T>> MpGDALReadT<T>::writeQueue_; // 用于缓存从磁盘读取的数据

        template <typename T>
        int MpGDALReadT<T>::readQueueMaxSize_ = 8;

        template <typename T>
        std::mutex MpGDALReadT<T>::mutexReadQueue_;

        template <typename T>
        std::mutex MpGDALReadT<T>::mutexWriteQueue_;

        template <typename T>
        std::condition_variable MpGDALReadT<T>::condReadQueueNotEmpty_;

        template <typename T>
        std::condition_variable MpGDALReadT<T>::condReadQueueNotFull_;
        // $1


    } // namespace Mp

} // namespace RSTool

#endif //IMGPROCESS_RSTOOL_DOUBLEBUFFERTEST_HPP
