//
// Created by penglei on 18-10-10.
//

#ifndef IMGPROCESS_MG_CUBE_H
#define IMGPROCESS_MG_CUBE_H

#include "mg_matcommon.h"

namespace Mg {

    class MgCube {
    public:
        MgCube();
        MgCube(int height, int width, int bands);

        MgCube(const MgCube &other);
        MgCube(MgCube &&other);

        MgCube& operator = (const MgCube& other);
        MgCube& operator = (MgCube&& other);

        Mat::Matrixd spectrum(int i, int j);
        void spectrum(int i, int j, const Mat::Matrixd &spec);

        Mat::ExtMatrixd& band(int i);

        Mat::ExtMatrixd& data();
        int width() const { return width_; }
        int height() const { return height_; }
        int bands() const { return bands_; }

        void resize(int height, int width, int bands);

    private:
        int height_;
        int width_;
        int bands_;
        std::shared_ptr<double> dataPtr_;

        // 数据矩阵，按 BSQ 存储
        std::vector<Mat::ExtMatrixd> cube_;
    };

} // namespace Mg

#endif //IMGPROCESS_MG_CUBE_H
