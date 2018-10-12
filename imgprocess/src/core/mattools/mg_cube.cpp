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
        dataPtr_(new double[height_*width_*bands]{},
                [](double*p){delete []p; p = nullptr;}) {

        for (int i = 0; i < bands_; ++i) {
            cube_.emplace_back(Mat::ExtMatrixd(
                    dataPtr_.get()+i*height_*width_,
                    height_, width_));
        }

        // 最后一个表示全部数据
        cube_.emplace_back(Mat::ExtMatrixd(
                dataPtr_.get(), bands_, height_*width_));
    }

    Mat::Matrixd MgCube::spectrum(int i, int j) {
        assert(i >= 0 && i < height_ && j >= 0 && j < width_);
        return cube_.back().col(i*width_+j);
    }

    void MgCube::spectrum(int i, int j, const Mat::Matrixd &spec) {
        assert(i >= 0 && i < height_ && j >= 0 && j < width_);
        cube_.back().col(i*width_+j) = spec;
    }

    Mat::ExtMatrixd& MgCube::band(int i) {
        assert(i >= 0 && i < bands_);
        return cube_[i];
    }

    Mat::ExtMatrixd& MgCube::data() {
        return cube_.back();
    }

    void MgCube::resize(int height, int width, int bands) {
        height_ = height;
        width_ = width;
        bands_ = bands;
        dataPtr_.reset(new double[height_*width_*bands]{},
                [](double*p){delete []p; p = nullptr;});

        cube_.clear();
        for (int i = 0; i < bands_; ++i) {
            cube_.emplace_back(Mat::ExtMatrixd(
                    dataPtr_.get()+height_*width_,
                    height_, width_));
        }

        // 最后一个表示全部数据
        cube_.emplace_back(Mat::ExtMatrixd(
                dataPtr_.get(), bands_, height_*width_));
    }

} // namespace Mg