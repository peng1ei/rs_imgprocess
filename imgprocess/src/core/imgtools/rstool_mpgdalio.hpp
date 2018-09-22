//
// Created by penglei on 18-9-21.
//

#ifndef IMGPROCESS_RSTOOL_MPGDALIO_HPP
#define IMGPROCESS_RSTOOL_MPGDALIO_HPP

#include "rstool_common.h"
#include "rstool_threadpool.h"

namespace RSTool {


    template <typename T>
    class MpGdalIO {
    public:
        MpGdalIO(const std::string &infile, const std::string &outfile,
                int blkSize = 128, int queueMaxSize = 16, int ioCount = 4)
                : infile_(infile), outfile_(outfile), poolCount_(ioCount),
                  pools_(poolCount_) {

            GDALAllRegister();

            poInDS_ = (GDALDataset*)GDALOpen(infile_.c_str(), GA_ReadOnly);
            imgXSize_ = poInDS_->GetRasterXSize();
            imgYSize_ = poInDS_->GetRasterYSize();
            imgBandCount_ = poInDS_->GetRasterCount();
            blkSize_ = blkSize;
            queueMaxSize_ = queueMaxSize;
            consumerCount_ = 4; // TODO 根据CPU核数确定
            tasks_.resize(consumerCount_);

            int xNUms = (imgXSize_ + blkSize_ - 1) / blkSize_;
            int yNUms = (imgYSize_ + blkSize_ - 1) / blkSize_;
            blkNums_ = xNUms*yNUms;

            // IO并行化
            for (int i = 0; i < poolCount_; i++) {
                GDALDataset * ds = (GDALDataset*)GDALOpen(infile.c_str(), GA_ReadOnly);
                datasets_.push_back(ds);
            }

        } // end MpGdalIO


        void assignWorkload() {

            // 将文件分块
            std::vector<RSTool::SpatialDims> dims;
            for (int i = 0; i < imgYSize_; i += blkSize_) {
                int yBlockSize = blkSize_;
                if (i + blkSize_ > imgYSize_) // 最下面的剩余块
                    yBlockSize = imgYSize_ - i;

                for (int j = 0; j < imgXSize_; j += blkSize_) {
                    int xBlockSize = blkSize_;
                    if (j + blkSize_ > imgXSize_) // 最右侧的剩余块
                        xBlockSize = imgXSize_ - j;

                    dims.emplace_back(RSTool::SpatialDims(j, i, xBlockSize, yBlockSize));
                } // end row
            } // end col

            // 分配工作量
            int xNUms = (imgXSize_ + blkSize_ - 1) / blkSize_;
            int yNUms = (imgYSize_ + blkSize_ - 1) / blkSize_;
            int perThreadYBlkNums = yNUms / poolCount_;
            int leftYNums = yNUms % poolCount_;

            int size = perThreadYBlkNums*xNUms; // 每一个线程需处理的任务量（块数）
            for (int i = 0; i < size; i++) {

                for (int j = 0; j < poolCount_; j++) {
                    GDALDataset *ds = datasets_[j];
                    RSTool::SpatialDims &dim = dims[j*perThreadYBlkNums+i];

                    int xOff = dim.xOff();
                    int yOff = dim.yOff();
                    int xSize = dim.xSize();
                    int ySize = dim.ySize();

                    pools_[j].enqueue([this, ds, xOff, yOff, xSize, ySize] {

                        auto start = std::chrono::high_resolution_clock::now();

                        RSTool::DataChunk<T> data(xOff, yOff, xSize, ySize, imgBandCount_);
                        RSTool::ReadDataChunk<T> read(ds, imgBandCount_);
                        if ( !read(xOff, yOff, xSize, ySize, data.data()) ) {
                            throw std::runtime_error("Reading data chunk is faild.");
                        }

                        auto end = std::chrono::high_resolution_clock::now();
                        std::chrono::duration<double, std::milli> elapsed = end-start;
                        std::cout<< "read: " << elapsed.count() << std::endl;

                        {
                            // 等待队列中有空闲位置
                            std::unique_lock<std::mutex> lk(mutexQueue_);
                            while (queue_.size() == queueMaxSize_) {
                                condQueueNotFull_.wait(lk);
                            }

                            queue_.emplace(std::move(data));
                        }
                        condQueueNotEmpty_.notify_all();

                    }); // end lambad
                }
            }

            // 将剩余的块交给最后一个线程池 IO 处理
            int leftNums = leftYNums*xNUms;
            for (int i = 0; i < leftNums; i++) {
                GDALDataset *ds = datasets_[poolCount_-1];
                RSTool::SpatialDims & dim = dims[poolCount_*size+i];
                int xOff = dim.xOff();
                int yOff = dim.yOff();
                int xSize = dim.xSize();
                int ySize = dim.ySize();

                pools_[poolCount_-1].enqueue([this, ds, xOff, yOff, xSize, ySize] {

                    auto start = std::chrono::high_resolution_clock::now();

                    RSTool::DataChunk<T> data(xOff, yOff, xSize, ySize, imgBandCount_);
                    RSTool::ReadDataChunk<T> read(ds, imgBandCount_);
                    if ( !read(xOff, yOff, xSize, ySize, data.data()) ) {
                        throw std::runtime_error("Reading data chunk is faild.");
                    }

                    auto end = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double, std::milli> elapsed = end-start;
                    std::cout << "read: "<< elapsed.count() << std::endl;

                    {
                        // 等待队列中有空闲位置
                        std::unique_lock<std::mutex> lk(mutexQueue_);
                        while (queue_.size() == queueMaxSize_) {
                            condQueueNotFull_.wait(lk);
                        }

                        queue_.emplace(std::move(data));
                    }
                    condQueueNotEmpty_.notify_all();

                }); // end lambad
            }

            // 每个 消费者线程 需要处理的任务量（块数）
            int perNums = blkNums_ / consumerCount_;
            int leftsNums = blkNums_ % consumerCount_;

            for (int i = 0; i < consumerCount_; i++) {
                tasks_[i] = perNums;
            }
            tasks_[consumerCount_-1] += leftsNums;

            // TODO： 每个 IO线程 处理的任务量

        } // end assignWorkload()

        void consumeTask(std::function<void(RSTool::DataChunk<T> &)> &&funcProcessDataCore, int tasks) {

            auto func = std::forward<std::function<void(DataChunk<T> &)>>(funcProcessDataCore);

            DataChunk<T> data(1,1,1,1,1); // 临时构造一个数据块
            for (int i = 0; i < tasks; ++i) {

                {
                    // 如果缓冲区中没有数据则扥带
                    std::unique_lock<std::mutex> lk(mutexQueue_);
                    while (queue_.empty()) {
                        condQueueNotEmpty_.wait(lk);
                    }

                    data = std::move(queue_.front());
                    queue_.pop();
                }
                condQueueNotFull_.notify_all();

                // TODO 核心操作，由用户实现
                func(data);
            }
        } // end consumeTask()

        template <class Fn, class... Args>
        void emplaceTask(Fn &&fn, Args &&... args) {
            consumerTasks_.emplace_back(std::bind(std::forward<Fn>(fn), std::placeholders::_1,
                    std::forward<Args>(args)...));
        }

    public:
        void run() {
            assignWorkload();

            std::cout << "pools size: " << sizeof(pools_[poolCount_-1]) << std::endl;

            for (int i = 0; i < consumerCount_; i++) {
                consumerThreads_.emplace_back(std::thread(&MpGdalIO<T>::consumeTask,
                        this, consumerTasks_[i], tasks_[i]));
            }

            // todo 如果不需要和主线程进行同步，是否可以分离线程？？？
            for (int i = 0; i < consumerCount_; i++) {
                consumerThreads_[i].join();
            }
        }

    private:
        std::queue<RSTool::DataChunk<T>> queue_;
        int queueMaxSize_;
        std::mutex mutexQueue_;
        std::condition_variable condQueueNotEmpty_;
        std::condition_variable condQueueNotFull_;

    private:
        int poolCount_; // IO线程 数量
        std::vector< Mp::ThreadPool> pools_;
        std::vector<GDALDataset*> datasets_;
        std::vector<int> tasks_;

    private:
        GDALDataset *poInDS_;
        //GDALDataset *poOutDS_;
        int imgBandCount_;
        int imgXSize_;
        int imgYSize_;

        int blkSize_;
        int blkNums_;

        std::string infile_;
        std::string outfile_;

    private:
        int consumerCount_; // 消费者线程数量
        std::vector<std::thread> consumerThreads_;   // 消费者线程（块数据处理线程）

        // 每个消费者线程所处理的核心函数对象
        // 可以从主线程中接收不同的参数（主要是为了将主线程的任务并行化）
        // 块处理的核心功能，由使用者负责实现，可传入 lambda、成员函数、全局函数、函数对象等
        std::vector< std::function<void(DataChunk<T> &)> > consumerTasks_;
    };

} // namespace RSTool

#endif //IMGPROCESS_RSTOOL_MPGDALIO_HPP
