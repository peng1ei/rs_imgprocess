//
// Created by penglei on 18-9-19.
//

#ifndef IMGPROCESS_IMGTOOL_THREADIO_HPP
#define IMGPROCESS_IMGTOOL_THREADIO_HPP

#include "imgtool_common.hpp"
#include "imgtool_progress.hpp"
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <chrono>
#include <string>
#include "ThreadPool.h"

class GDALDataset;

namespace ImgTool {

    namespace Mp {

        template <typename T>
        struct BufferQueue {
            std::vector<ImgBlockData<T>> items_;
            int itemsMaxSize_; // 缓冲区队列最大的 item 数量

            std::mutex mutexItems_;
            std::condition_variable condBufNotFull_;
            std::condition_variable condBufNotEmpty_;

            size_t consumedItemCount_;  // 已消费的产品数
            std::mutex mutexConsumedItemCount_;

            int produceItemCount_; // 产品总量

            size_t readPos_;
            size_t  writePos_;
        };

        std::mutex mtx;

        template <typename T>
        class MpThreadIO {
        public:
            //using VecThreadRet = std::vector< std::future< ImgBlockData<T> >>;
            using VecThreadRet = std::vector< std::future<bool>>;

        public:
            MpThreadIO(const std::string &infile, const std::string &outfile)
                    : infile_(infile), outfile_(outfile) {

                GDALAllRegister();

                poInDS_ = (GDALDataset*)GDALOpen(infile_.c_str(), GA_ReadOnly);
                imgXSize_ = poInDS_->GetRasterXSize();
                imgYSize_ = poInDS_->GetRasterYSize();
                imgBandCount_ = poInDS_->GetRasterCount();
                blkSize_ = 128;

                // TODO 根据CPU核数计算线程数
                consumers_ = 4;
                producerRets_.resize(consumers_);

                // 总块数
                int xNUms = (imgXSize_ + blkSize_ - 1) / blkSize_;
                int yNUms = (imgYSize_ + blkSize_ - 1) / blkSize_;
                blkNums_ =xNUms*yNUms;

                // 初始化 queue
                queue_.readPos_ = 0;
                queue_.writePos_ = 0;
                queue_.produceItemCount_ = blkNums_;
                queue_.consumedItemCount_ = 0;
                queue_.itemsMaxSize_ = 8;
                for (int i = 0; i < queue_.itemsMaxSize_; i++)
                    queue_.items_.emplace_back(ImgBlockData<T>(ImgSpatialSubset(),
                            ImgSpectralSubset(imgBandCount_), blkSize_, blkSize_));

                time_ = 0;
            }

            void produceTask() {
                // TODO 设置一个独立线程，用于管理线程池
                ThreadPool pool(1);

                // 线程池生产数据
                int count = 0;
                int blkElemCount = blkSize_*blkSize_*imgBandCount_;
                for (int i = 0; i < imgYSize_; i += blkSize_) {
                    int yBlockSize = blkSize_;
                    if (i + blkSize_ > imgYSize_) // 最下面的剩余块
                        yBlockSize = imgYSize_ - i;

                    for (int j = 0; j < imgXSize_; j += blkSize_) {
                        int xBlockSize = blkSize_;
                        if (j + blkSize_ > imgXSize_) // 最右侧的剩余块
                            xBlockSize = imgXSize_ - j;

                        {
                            // 有限缓冲区，默认设置为 16 个
                            std::unique_lock<std::mutex> lock(queue_.mutexItems_);
                            queue_.condBufNotFull_.wait(lock, [this]{
                                return (((queue_.writePos_+1)%queue_.itemsMaxSize_) != queue_.readPos_);
                            }); // 缓冲区已满，就等待消费者从缓冲区取走一个数据
                        }

                        // TODO 将产生的数据分别添加至不同的处理线程中
                        producerRets_[((count++) % 4)].emplace_back(
                                pool.enqueue([this, j, i, xBlockSize, yBlockSize, blkElemCount] {

                                    //std::cout << "<" << j << ", " << i << ">" << std::endl;



                                    GDALDataset *inDS = (GDALDataset*)GDALOpen(infile_.c_str(), GA_ReadOnly);
                                    if (inDS == nullptr)
                                        return false;

                                    T *buf = new T[blkElemCount]{};

                                    //auto start = std::chrono::high_resolution_clock::now();

                                    if ( CPLErr::CE_Failure == inDS->RasterIO(GF_Read,
                                            j, i, xBlockSize, yBlockSize, buf, xBlockSize, yBlockSize,
                                            GDT_UInt16, imgBandCount_, 0,
                                            sizeof(T)*imgBandCount_,
                                            sizeof(T)*imgBandCount_*xBlockSize,
                                            sizeof(T)) ) {
                                        return false;
                                    }
                                    GDALClose((GDALDatasetH)inDS);

                                    //auto end = std::chrono::high_resolution_clock::now();
                                    //std::chrono::duration<double, std::milli> elapsed = end-start;
                                    //std::cout << elapsed.count() << std::endl;

                                    //mtx.lock();
                                    //time_ += elapsed.count();
                                    //mtx.unlock();

                                    {
                                        std::unique_lock<std::mutex> lock(queue_.mutexItems_);

                                        // TODO 交换队列中的指针
                                        queue_.items_[queue_.writePos_].updateSpatial(j, i, xBlockSize, yBlockSize);
                                        queue_.items_[queue_.writePos_].updataBufData(buf);


                                        queue_.writePos_++;
                                        if (queue_.writePos_ == queue_.itemsMaxSize_)
                                            queue_.writePos_ = 0;
                                    }

                                    ReleaseArray(buf);
                                    return true;
                                }));

                    } // end row
                } // end col

            } // end produceTask()

            void consumeTask(VecThreadRet &results) {

                double *mean = new double[imgBandCount_]{};
                double *stdDev = new double[imgBandCount_]{};
                double *covariance = new double[imgBandCount_*imgBandCount_]{};

                ImgBlockData<T> data(ImgSpatialSubset(), ImgSpectralSubset(imgBandCount_), blkSize_, blkSize_);
                for(auto && result: results) {

                    //ImgBlockData<T> data = result.get();
                    if (result.get()) {
                        // 从缓冲区取一个数据
                        {
                            std::unique_lock<std::mutex> lock(queue_.mutexItems_);
                            data.updateSpatial(queue_.items_[queue_.readPos_].spatial());
                            data.updataBufData(queue_.items_[queue_.readPos_].bufData());

                            queue_.readPos_++;
                            if (queue_.readPos_ >= queue_.itemsMaxSize_)
                                queue_.readPos_ = 0;
                            queue_.condBufNotFull_.notify_all();
                        }
                    }

                    auto start = std::chrono::high_resolution_clock::now();

                    int size = data.spatial().xSize()*data.spatial().ySize();
                    T *buf = data.bufData();
                    T *pBuf1, *pBuf2;

                    for (int i = 0; i < size; i++) {
                        int index = i*imgBandCount_;
                        pBuf1 = buf + index;

                        for (int b = 0; b < imgBandCount_; b++) {
                            pBuf2 = buf + b;

                            // sum of X: x1 + x2 + x3 + ...
                            mean[b] += pBuf2[index];

                            // sum of X^2: x1^2 + x2^2 + x3^2 + ...
                            stdDev[b] += pBuf2[index]*pBuf2[index];

                            // sum of X*Y: x1*y1 + x2*y2 + x3*y3 + ...
                            double *pCovar = covariance + b*imgBandCount_;
                            for (int b1 = b; b1 < imgBandCount_; b1++) {
                                pCovar[b1] += pBuf1[b1]*pBuf2[index];
                            }
                        }

                    }

                    auto end = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double, std::milli> elapsed = end-start;
                    std::cout << elapsed.count() << std::endl;

                }// end results

                delete [] mean;
                delete [] stdDev;
                delete [] covariance;

            } // end consumeTask()

            void ioTime() {
                std::cout << time_ << " ms" << std::endl;
            }

        public:
            void run() {
                std::thread producer(&MpThreadIO<T>::produceTask, this);

                for (int i = 0; i < consumers_; i++) {
                    consumerThreads_.emplace_back(std::thread(&MpThreadIO<T>::consumeTask, this,
                            std::ref(producerRets_[i])));
                }

                producer.join();
                for (auto &&consumer : consumerThreads_) {
                    consumer.join();
                }
            };

        private:
            BufferQueue<T> queue_;

            std::vector< VecThreadRet > producerRets_; // 线程池返回的结果
            int consumers_;
            std::vector<std::thread> consumerThreads_;

            double time_;

        private:
            GDALDataset *poInDS_;
            GDALDataset *poOutDS_;

            int imgBandCount_;
            int imgXSize_;
            int imgYSize_;

            int blkSize_;
            int blkNums_;

            std::string infile_;
            std::string outfile_;
        };
    }
} // namespace ImgTool

#endif //IMGPROCESS_IMGTOOL_THREADIO_HPP
