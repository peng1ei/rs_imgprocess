//
// Created by penglei on 18-9-19.
//

#ifndef IMGPROCESS_IMGTOOL_THREADIO_HPP
#define IMGPROCESS_IMGTOOL_THREADIO_HPP

#include "imgtool_common.hpp"
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

        struct Dims {
            Dims(int xOff, int yOff, int xSize, int ySize)
                : xOff_(xOff), yOff_(yOff), xSize_(xSize), ySize_(ySize) {}
            int xOff_;
            int yOff_;
            int xSize_;
            int ySize_;
        };

        template <typename T>
        class MpThreadIO {
        public:
            //using VecThreadRet = std::vector< std::future< ImgBlockData<T> >>;
            using VecThreadRet = std::vector< std::future<T*>>;

        public:
            MpThreadIO(const std::string &infile, const std::string &outfile)
                    : infile_(infile), outfile_(outfile), poolThreadsCount_(4), pool_(1),
                    pools_(poolThreadsCount_) {

                GDALAllRegister();

                poInDS_ = (GDALDataset*)GDALOpen(infile_.c_str(), GA_ReadOnly);
                imgXSize_ = poInDS_->GetRasterXSize();
                imgYSize_ = poInDS_->GetRasterYSize();
                imgBandCount_ = poInDS_->GetRasterCount();
                blkSize_ = 128;

                // TODO 根据CPU核数计算线程数
                consumers_ = 4;
                producerRets_.resize(consumers_);
                consumerItemsCount_.resize(consumers_);

                // 总块数
                int xNUms = (imgXSize_ + blkSize_ - 1) / blkSize_;
                int yNUms = (imgYSize_ + blkSize_ - 1) / blkSize_;
                blkNums_ =xNUms*yNUms;

                // 计算各个线程的任务数
                int base = blkNums_ / consumers_;
                for (auto &value : consumerItemsCount_) {
                    value = base;
                }

                int model = blkNums_ % consumers_;
                switch (model) {
                    case 1:
                        consumerItemsCount_[0] += 1;
                        break;
                    case 2:
                        consumerItemsCount_[0] += 1;
                        consumerItemsCount_[1] += 1;
                        break;
                    case 3:
                        consumerItemsCount_[0] += 1;
                        consumerItemsCount_[1] += 1;
                        consumerItemsCount_[2] += 1;
                        break;
                }


                // 初始化 queue
//                queue_.readPos_ = 0;
//                queue_.writePos_ = 0;
//                queue_.produceItemCount_ = blkNums_;
//                queue_.consumedItemCount_ = 0;
//                queue_.itemsMaxSize_ = 32;
//                for (int i = 0; i < queue_.itemsMaxSize_; i++)
//                    queue_.items_.emplace_back(ImgBlockData<T>(ImgSpatialSubset(),
//                            ImgSpectralSubset(imgBandCount_), blkSize_, blkSize_));

                // 初始化线程池
                for (int i = 0; i < poolThreadsCount_; i++) {
                    GDALDataset * ds = (GDALDataset*)GDALOpen(infile.c_str(), GA_ReadOnly);
                    datasets_.push_back(ds);
                }
            }

            ~MpThreadIO() {
                std::cout << "IO virtual" << std::endl;
            }

            void produceTask() {

                std::vector<Dims> dims;

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

                        dims.emplace_back(Dims(j, i, xBlockSize, yBlockSize));
                    } // end row
                } // end col


                int xNUms = (imgXSize_ + blkSize_ - 1) / blkSize_;
                int yNUms = (imgYSize_ + blkSize_ - 1) / blkSize_;
                int blkSize = yNUms / poolThreadsCount_;
                int lefts = yNUms % poolThreadsCount_;



                int imgBandCount = imgBandCount_;

                int index = 0;
                int size = blkSize*xNUms;
                for (int i = 0; i < size; i++) {

                    for (int j = 0; j < poolThreadsCount_; j++) {
                        GDALDataset *ds = datasets_[j];
                        Dims & dim = dims[j*blkSize+i];
                        int xOff = dim.xOff_;
                        int yOff = dim.yOff_;
                        int xSize = dim.xSize_;
                        int ySize = dim.ySize_;
                        producerRets_[j].emplace_back(
                                pools_[j].enqueue([ds, imgBandCount, xOff, yOff, xSize, ySize] {

                            T *buf = new T[xSize*ySize*imgBandCount]{};

                            auto start = std::chrono::high_resolution_clock::now();
                            if ( CPLErr::CE_Failure == ds->RasterIO(GF_Read,
                                    xOff, yOff, xSize, ySize, buf, xSize, ySize,
                                    GDT_UInt16, imgBandCount, 0,
                                    sizeof(T)*imgBandCount,
                                    sizeof(T)*imgBandCount*xSize,
                                    sizeof(T)) ) {
                                return (T*) nullptr;
                            }

                            auto end = std::chrono::high_resolution_clock::now();
                            std::chrono::duration<double, std::milli> elapsed = end-start;
                            std::cout << elapsed.count() << std::endl;

                            //ReleaseArray(buf);
                            return buf;
                        }));
                    }
                }

                int leftSize = lefts*xNUms;
                for (int i = 0; i < leftSize; i++) {
                    GDALDataset *ds = datasets_[poolThreadsCount_-1];
                    Dims & dim = dims[poolThreadsCount_*size+i];
                    int xOff = dim.xOff_;
                    int yOff = dim.yOff_;
                    int xSize = dim.xSize_;
                    int ySize = dim.ySize_;
                    producerRets_[poolThreadsCount_-1].emplace_back(
                            pools_[poolThreadsCount_-1].enqueue([ds, imgBandCount, xOff, yOff, xSize, ySize] {

                                T *buf = new T[xSize*ySize*imgBandCount]{};

                                auto start = std::chrono::high_resolution_clock::now();
                                if ( CPLErr::CE_Failure == ds->RasterIO(GF_Read,
                                        xOff, yOff, xSize, ySize, buf, xSize, ySize,
                                        GDT_UInt16, imgBandCount, 0,
                                        sizeof(T)*imgBandCount,
                                        sizeof(T)*imgBandCount*xSize,
                                        sizeof(T)) ) {
                                    return (T*) nullptr;
                                }

                                auto end = std::chrono::high_resolution_clock::now();
                                std::chrono::duration<double, std::milli> elapsed = end-start;
                                std::cout << elapsed.count() << std::endl;

                                //ReleaseArray(buf);
                                return buf;
                            }));
                }


                std::cout << "------------------end producer-----------------" << std::endl;
            } // end produceTask()

            void consumeTask(VecThreadRet &results, int itemsCount) {
                double *mean = new double[imgBandCount_]{};
                double *stdDev = new double[imgBandCount_]{};
                double *covariance = new double[imgBandCount_*imgBandCount_]{};

//                int count = 0;
//
//                itemsCount -= 1;
//
//                // 等待任务
//                while (results.empty()) {}
//
//                // 有任务
//                while (true) {
////                    if (results.empty())
////                        continue;
//
//                    auto itbeg = results.begin();
//                    auto itend = results.end();
//
//                    std::advance(itbeg, count);
//                    if (itbeg != itend) {
//                        // TODO 处理
//                        //auto &future = *itbeg;
//
//                        T *buf;
//                        try {
//                            buf = itbeg->get();
//
//                        }
//                        catch (std::exception &e) {
//                            std::cout << "get(): " << e.what() << std::endl;
//                            return;
//                        }
//
//                        int size = blkSize_*blkSize_;
//                        T *pBuf1, *pBuf2;
//
//                        auto start = std::chrono::high_resolution_clock::now();
//
//                        for (int i = 0; i < size; i++) {
//                            int index = i*imgBandCount_;
//                            pBuf1 = buf + index;
//
//                            for (int b = 0; b < imgBandCount_; b++) {
//                                pBuf2 = buf + b;
//
//                                // sum of X: x1 + x2 + x3 + ...
//                                mean[b] += pBuf2[index];
//
//                                // sum of X^2: x1^2 + x2^2 + x3^2 + ...
//                                stdDev[b] += pBuf2[index]*pBuf2[index];
//
//                                // sum of X*Y: x1*y1 + x2*y2 + x3*y3 + ...
//                                double *pCovar = covariance + b*imgBandCount_;
//                                for (int b1 = b; b1 < imgBandCount_; b1++) {
//                                    pCovar[b1] += pBuf1[b1]*pBuf2[index];
//                                }
//                            }
//
//                        }
//
//                        delete [] buf;
//                        buf = nullptr;
//
//                        auto end = std::chrono::high_resolution_clock::now();
//                        std::chrono::duration<double, std::milli> elapsed = end-start;
//                        std::cout << elapsed.count() << std::endl;
//
//                        count++;
//                        continue;
//                    }
////                    else {
////                        continue;
////                    }
//
//                    if (count == itemsCount)
//                        break;
//
//                    //count++;
//                }
//
//                delete [] mean;
//                delete [] stdDev;
//                delete [] covariance;
//
//                return;


                //ImgBlockData<T> data(ImgSpatialSubset(), ImgSpectralSubset(imgBandCount_), blkSize_, blkSize_);
                for(auto && result: results) {

                    //ImgBlockData<T> data = result.get();
//                    if (result.get()) {
//                        // 从缓冲区取一个数据
//                        {
//                            std::unique_lock<std::mutex> lock(queue_.mutexItems_);
//                            data.updateSpatial(queue_.items_[queue_.readPos_].spatial());
//                            data.updataBufData(queue_.items_[queue_.readPos_].bufData());
//
//                            queue_.readPos_++;
//                            if (queue_.readPos_ >= queue_.itemsMaxSize_)
//                                queue_.readPos_ = 0;
//                            queue_.condBufNotFull_.notify_all();
//                        }
//                    }



                    //int size = data.spatial().xSize()*data.spatial().ySize();
                    //T *buf = data.bufData();

//                    while (!result.valid()) {
//                        std::cout << "valid" << std::endl;
//                    };

                    T *buf = result.get();
                    ReleaseArray(buf);
                    continue;
                    //T *buf;
                    int size = blkSize_*blkSize_;
                    T *pBuf1, *pBuf2;

                    auto start = std::chrono::high_resolution_clock::now();

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

                    delete [] buf;
                    buf = nullptr;

                    //auto end = std::chrono::high_resolution_clock::now();
                    //std::chrono::duration<double, std::milli> elapsed = end-start;
                    //std::cout << elapsed.count() << std::endl;

                }// end results

                delete [] mean;
                delete [] stdDev;
                delete [] covariance;

            } // end consumeTask()

            void consumeTask1(VecThreadRet &results) {


                int numThreads = 4;

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
                            queue_.condBufNotFull_.notify_one();
                        }
                    }

                    auto start = std::chrono::high_resolution_clock::now();

                    int size = data.spatial().xSize()*data.spatial().ySize();
                    int blockSize = size / numThreads;
                    std::vector<std::thread> threads(numThreads);

                    int blockStart = 0;
                    T *buf = data.bufData();

                    for (int i = 0; i < numThreads; i++) {
                        int end = blockStart + blockSize;

                        threads[i] = std::thread([this, buf, blockStart, end]{
                            double *mean = new double[imgBandCount_]{};
                            double *stdDev = new double[imgBandCount_]{};
                            double *covariance = new double[imgBandCount_*imgBandCount_]{};

                            //T *buf = data.bufData();
                            T *pBuf1, *pBuf2;

                            for (int i = blockStart; i < end; i++) {
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

                            delete [] mean;
                            delete [] stdDev;
                            delete [] covariance;
                        });

                        blockStart = end;
                    }// end for

                    auto end = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double, std::milli> elapsed = end-start;
                    std::cout << elapsed.count() << std::endl;

                }// end results



            } // end consumeTask()

            void ioTime() {
                std::cout << time_ << " ms" << std::endl;
            }

        public:
            void run() {

                //std::cout << "------------------begin assignment task-----------------" << std::endl;
                //auto start = std::chrono::high_resolution_clock::now();
                std::thread producer(&MpThreadIO<T>::produceTask, this);
                producer.join();

                //produceTask();

                //auto end = std::chrono::high_resolution_clock::now();
                //std::chrono::duration<double, std::milli> elapsed = end-start;
                //std::cout << elapsed.count() << std::endl;
                //std::cout << "------------------end assignment task-----------------\n" << std::endl;

                for (int i = 0; i < consumers_; i++) {
                    consumerThreads_.emplace_back(std::thread(&MpThreadIO<T>::consumeTask, this,
                            std::ref(producerRets_[i]), consumerItemsCount_[i]));
                }

                //std::thread consumer(&MpThreadIO<T>::consumeTask1, this,
                //        std::ref(producerRets_[0]));


                //consumer.join();
                //
                for (auto &&consumer : consumerThreads_) {
                    consumer.join();
                }
            }

        private:
            BufferQueue<T> queue_;

            std::vector< VecThreadRet > producerRets_; // 线程池返回的结果
            int consumers_;
            std::vector<std::thread> consumerThreads_;
            std::vector<int> consumerItemsCount_;

            double time_;

            size_t poolThreadsCount_;
            ThreadPool pool_;
            std::vector<ThreadPool> pools_;
            std::vector<GDALDataset*> datasets_;

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
