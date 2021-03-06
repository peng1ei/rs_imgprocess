//
// Created by penglei on 18-10-10.
//

#include "mg_cube.h"
#include <cassert>
#include <iostream>

namespace Mg {

    MgCube::MgCube()
        : height_(0), width_(0), bands_(0),
        dataPtr_(nullptr) {
    }

    MgCube::MgCube(int height, int width, int bands)
        : height_(height), width_(width), bands_(bands),
        dataPtr_(new float[height_*width_*bands]{},
                [](float*p){delete []p; p = nullptr;}) {

        for (int i = 0; i < bands_; ++i) {
            cube_.emplace_back(Mat::ExtMatrixf(
                    dataPtr_.get()+i*height_*width_,
                    height_, width_));
        }

        // 最后一个表示全部数据
        cube_.emplace_back(Mat::ExtMatrixf(
                dataPtr_.get(), bands_, height_*width_));
    }

    MgCube::MgCube(const Mg::MgCube &other)
        : height_(other.height_), width_(other.width_), bands_(other.bands_) {

        dataPtr_.reset(new float[height_*width_*bands_]{},
                       [](float*p){delete []p; p = nullptr;});

        mempcpy(dataPtr_.get(), other.dataPtr_.get(),
                sizeof(float)*height_*width_*bands_);

        cube_.clear();
        for (int i = 0; i < bands_; ++i) {
            cube_.emplace_back(Mat::ExtMatrixf(
                    dataPtr_.get()+i*height_*width_,
                    height_, width_));
        }

        // 最后一个表示全部数据
        cube_.emplace_back(Mat::ExtMatrixf(
                dataPtr_.get(), bands_, height_*width_));

    }

    MgCube::MgCube(MgCube &&other)
            : height_(other.height_), width_(other.width_), bands_(other.bands_),
              dataPtr_(std::move(other.dataPtr_)), cube_(std::move(other.cube_)) {

        other.cube_.clear();
        other.height_ = 0;
        other.width_ = 0;
        other.bands_ = 0;

        /*
        cube_.clear();
        for (int i = 0; i < bands_; ++i) {
            cube_.emplace_back(Mat::ExtMatrixd(
                    dataPtr_.get()+i*height_*width_,
                    height_, width_));
        }

        // 最后一个表示全部数据
        cube_.emplace_back(Mat::ExtMatrixd(
                dataPtr_.get(), bands_, height_*width_));
        */
    }

    MgCube& MgCube::operator = (const MgCube& other) {
        height_ = other.height_;
        width_ = other.width_;
        bands_ = other.bands_;

        dataPtr_.reset(new float[height_*width_*bands_]{},
                       [](float*p){delete []p; p = nullptr;});

        mempcpy(dataPtr_.get(), other.dataPtr_.get(),
                sizeof(float)*height_*width_*bands_);

        cube_.clear();
        for (int i = 0; i < bands_; ++i) {
            cube_.emplace_back(Mat::ExtMatrixf(
                    dataPtr_.get()+i*height_*width_,
                    height_, width_));
        }

        // 最后一个表示全部数据
        cube_.emplace_back(Mat::ExtMatrixf(
                dataPtr_.get(), bands_, height_*width_));
    }

    MgCube& MgCube::operator = (MgCube&& other) {
        height_ = other.height_;
        width_ = other.width_;
        bands_ = other.bands_;

        dataPtr_ = std::move(other.dataPtr_);
        other.cube_.clear();
        other.height_ = 0;
        other.width_ = 0;
        other.bands_ = 0;

        cube_.clear();
        for (int i = 0; i < bands_; ++i) {
            cube_.emplace_back(Mat::ExtMatrixf(
                    dataPtr_.get()+i*height_*width_,
                    height_, width_));
        }

        // 最后一个表示全部数据
        cube_.emplace_back(Mat::ExtMatrixf
        (
                dataPtr_.get(), bands_, height_*width_));
    }


    Mat::Matrixf MgCube::spectrum(int i, int j) {
        assert(i >= 0 && i < height_ && j >= 0 && j < width_);
        return cube_.back().col(i*width_+j);
    }

    void MgCube::spectrum(int i, int j, const Mat::Matrixf &spec) {
        assert(i >= 0 && i < height_ && j >= 0 && j < width_);
        cube_.back().col(i*width_+j) = spec;
    }

    Mat::ExtMatrixf& MgCube::band(int i) {
        assert(i >= 0 && i < bands_);
        return cube_[i];
    }

    Mat::ExtMatrixf& MgCube::data() {
        return cube_.back();
    }

    void MgCube::resize(int height, int width, int bands) {
        height_ = height;
        width_ = width;
        bands_ = bands;
        dataPtr_.reset(new float[height_*width_*bands]{},
                [](float*p){delete []p; p = nullptr;});

        cube_.clear();
        for (int i = 0; i < bands_; ++i) {
            cube_.emplace_back(Mat::ExtMatrixf(
                    dataPtr_.get()+i*height_*width_,
                    height_, width_));
        }

        // 最后一个表示全部数据
        cube_.emplace_back(Mat::ExtMatrixf(
                dataPtr_.get(), bands_, height_*width_));
    }

} // namespace Mg