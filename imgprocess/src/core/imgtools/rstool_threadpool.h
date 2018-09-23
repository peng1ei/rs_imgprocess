//
// Created by penglei on 18-9-21.
//

#ifndef IMGPROCESS_RSTOOL_THREADPOOL_H
#define IMGPROCESS_RSTOOL_THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <iostream>

#include "rstool_common.h"

namespace RSTool {

    namespace Mp {

        /**
         * 根据 CPU核数,获取最佳线程数量
         * @param minPerThread  每个线程最小处理的任务量
         * @param numTasks      总共的任务量
         * @return              返回最佳的线程数量
         */
        inline int NumThreads(int minPerThread, int numTasks) {
            // 根据 CPU核数 确定最佳线程数量
            int hardThreads = std::thread::hardware_concurrency();
            int maxThreads = (numTasks + minPerThread - 1) / minPerThread;
            return std::min(hardThreads != 0 ? hardThreads : 1, maxThreads);
        }

        class ThreadPool {
        public:
            ThreadPool();
            ThreadPool(size_t);
            template<class F, class... Args>
            auto enqueue(F&& f, Args&&... args)
            -> std::future<typename std::result_of<F(Args...)>::type>;
            ~ThreadPool();

        private:
            // need to keep track of threads so we can join them
            std::vector< std::thread > workers;
            // the task queue
            std::queue< std::function<void()> > tasks;

            // synchronization
            std::mutex queue_mutex;
            std::condition_variable condition;
            bool stop;
        };

        // the constructor just launches some amount of workers
        inline ThreadPool::ThreadPool(size_t threads)
                :   stop(false)
        {
            for(size_t i = 0;i<threads;++i)
                workers.emplace_back(
                        [this]
                        {
                            for(;;)
                            {
                                std::function<void()> task;

                                {
                                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                                    this->condition.wait(lock,
                                                         [this]{ return this->stop || !this->tasks.empty(); });
                                    if(this->stop && this->tasks.empty())
                                        return;
                                    task = std::move(this->tasks.front());
                                    this->tasks.pop();
                                }

                                task();
                            }
                        }
                );
        }

        inline ThreadPool::ThreadPool() : stop(false) {
            workers.emplace_back(
                    [this] {
                        for(;;)
                        {
                            std::function<void()> task;

                            {
                                std::unique_lock<std::mutex> lock(this->queue_mutex);
                                this->condition.wait(lock,
                                                     [this]{ return this->stop || !this->tasks.empty(); });
                                if(this->stop && this->tasks.empty())
                                    return;
                                task = std::move(this->tasks.front());
                                this->tasks.pop();
                            }

                            task();
                        }
                    } // end lambda
            );
        }

        // add new work item to the pool
        template<class F, class... Args>
        auto ThreadPool::enqueue(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>
        {
            using return_type = typename std::result_of<F(Args...)>::type;

            auto task = std::make_shared< std::packaged_task<return_type()> >(
                    std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );

            std::future<return_type> res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(queue_mutex);

                // don't allow enqueueing after stopping the pool
                if(stop)
                    throw std::runtime_error("enqueue on stopped ThreadPool");

                tasks.emplace([task](){ (*task)(); });
            }
            condition.notify_one();
            return res;
        }

        // the destructor joins all threads
        inline ThreadPool::~ThreadPool()
        {
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                stop = true;
            }
            condition.notify_all();
            for(std::thread &worker: workers)
                worker.join();
        }

        template <typename T>
        class MpGDALRead {
        public:
            static std::queue<DataChunk<T>> readQueue_; // 用于缓存从磁盘读取的数据
            static int readQueueMaxSize_;
            static std::mutex mutexReadQueue_;
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
            MpGDALRead(const std::string &infile, const SpectralDimes &specDims,
                    Interleave &intl = Interleave::BIP, int poolsCount = 4)
                : infile_(infile), specDims_(specDims), intl_(intl),
                poolsCount_(poolsCount), pools_(poolsCount_), datasets_(poolsCount_) {

                for (auto &ds : datasets_) {
                    ds = (GDALDataset*)GDALOpen(infile.c_str(), GA_ReadOnly);
                }
            }

            ~MpGDALRead() {
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

                    auto start = std::chrono::high_resolution_clock::now();

                    DataChunk<T> data(spatDims, specDims_, intl_);
                    ReadDataChunk<T> read(ds, specDims_, intl_);
                    if ( !read(spatDims.xOff(), spatDims.yOff(),
                               spatDims.xSize(), spatDims.ySize(), data.data())) {
                        throw std::runtime_error("Reading data chunk is faild.");
                    }

                    auto end = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double, std::milli> elapsed = end-start;
                    std::cout<< "read: " << elapsed.count() << std::endl;

                    {
                        // 等待队列中有空闲位置
                        std::unique_lock<std::mutex> lk(mutexReadQueue_);
                        while (readQueue_.size() == readQueueMaxSize_) {
                            condReadQueueNotFull_.wait(lk);
                        }

                        readQueue_.emplace(std::move(data));
                    }
                    condReadQueueNotEmpty_.notify_all();

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

        template <typename T>
        int MpGDALRead<T>::readQueueMaxSize_ = 16;

        template <typename T>
        class MpGDALWrite {
        public:
            static std::queue<DataChunk<T>> writeQueue_; // 用于缓存输出至磁盘的数据
            static int writeQueueMaxSize_;
            static std::mutex mutexWriteQueue_;
            static std::condition_variable condWriteQueueNotEmpty_;
            static std::condition_variable condWriteQueueNotFull_;
            static bool stop;

        public:
            /**
             *
             * @param outfile       输出文件
             * @param specDims      输出的光谱范围
             * @param intl          输出文件在磁盘中的组织方式
             * @param poolsCount    写线程数量
             */
            MpGDALWrite(const std::string &outfile, int poolsCount = 4)
                    : outfile_(outfile), poolsCount_(poolsCount), pools_(poolsCount_),
                    datasets_(poolsCount_) {

                for (int i = 0; i < poolsCount; i++) {
                    GDALDataset *ds = (GDALDataset*)GDALOpen(outfile_.c_str(), GA_Update);
                    datasets_[i] = ds;
                    pools_[i].enqueue([ds] {

                        DataChunk<T> data(0, 0, 1, 1, 1); // 临时数据块
                        for (;;) {
                            {
                                // 如果缓冲区中没有数据,则等待数据的到来
                                std::unique_lock<std::mutex> lk(MpGDALWrite<T>::mutexWriteQueue_);
                                while (MpGDALWrite<T>::writeQueue_.empty() && !stop) {
                                    MpGDALWrite<T>::condWriteQueueNotEmpty_.wait(lk);
                                }

                                if (stop) return;

                                data = std::move(MpGDALWrite<T>::writeQueue_.front());
                                MpGDALWrite<T>::writeQueue_.pop();
                            }
                            MpGDALWrite<T>::condWriteQueueNotFull_.notify_all();

                            auto start = std::chrono::high_resolution_clock::now();

                            // 各个写线程“随机”写数据块
                            WriteDataChunk<T> write(ds);
                            if ( !write(data) ) {
                                throw std::runtime_error("Writing data chunk is faild.");
                            }

                            auto end = std::chrono::high_resolution_clock::now();
                            std::chrono::duration<double, std::milli> elapsed = end-start;
                            std::cout<< "write: " << elapsed.count() << std::endl;
                        }

                    }); // end lambad
                }
            } // end MpGDALWrite()

            ~MpGDALWrite() {
                for (auto &ds : datasets_) {
                    GDALClose((GDALDatasetH)ds);
                }
            }

        private:
            std::string outfile_;

        private:
            int poolsCount_; // 写线程 数量
            std::vector<ThreadPool> pools_;
            std::vector<GDALDataset*> datasets_;
        };

        template <typename T>
        int MpGDALWrite<T>::writeQueueMaxSize_ = 16;

        template <typename T>
        bool MpGDALWrite<T>::stop = false;

    } // namespace Mp

} // namespace RSTool

#endif //IMGPROCESS_RSTOOL_THREADPOOL_H
