//
// Created by penglei on 18-10-10.
//

#ifndef IMGPROCESS_MG_MATCOMMON_H
#define IMGPROCESS_MG_MATCOMMON_H

#include "Eigen/Dense"
#include <vector>
#include <memory>

using namespace Eigen;

namespace Mg {

    namespace Mat {

        // 数据存储在矩阵内部
        using Matrixub = Eigen::Matrix<unsigned char, Dynamic, Dynamic, RowMajor>;
        using Matrixus = Eigen::Matrix<unsigned short, Dynamic, Dynamic, RowMajor>;
        using Matrixs  = Eigen::Matrix<short, Dynamic, Dynamic, RowMajor>;
        using Matrixui = Eigen::Matrix<unsigned int, Dynamic, Dynamic, RowMajor>;
        using Matrixi  = Eigen::Matrix<int, Dynamic, Dynamic, RowMajor>;
        using Matrixf  = Eigen::Matrix<float, Dynamic, Dynamic, RowMajor>;
        using Matrixd  = Eigen::Matrix<double, Dynamic, Dynamic, RowMajor>;

        template <typename Scalar>
        using Matrix  = Eigen::Matrix<Scalar, Dynamic, Dynamic, RowMajor>;

        // 数据存储在外部（比如存储在外部数组中）
        using ExtMatrixub = Eigen::Map<Matrixub>;
        using ExtMatrixus = Eigen::Map<Matrixus>;
        using ExtMatrixs  = Eigen::Map<Matrixs>;
        using ExtMatrixui = Eigen::Map<Matrixui>;
        using ExtMatrixi  = Eigen::Map<Matrixi>;
        using ExtMatrixf  = Eigen::Map<Matrixf>;
        using ExtMatrixd  = Eigen::Map<Matrixd>;

        template <typename Scalar>
        using ExtMatrix  = Eigen::Map<Matrix<Scalar>>;

        /**
         * 判断矩阵是否可逆
         * @tparam _MatrixType  矩阵类型
         * @param mat           待判断的矩阵（注意不能为“Ext***”类型，否则会修改原矩阵数据）
         * @return 矩阵可逆就返回 true，不可逆就返回false
         */
        template <typename MatrixType>
        inline bool isInvertible(const MatrixType &mat) {
            return Eigen::FullPivLU<MatrixType>(mat).isInvertible();
        }

        /**
         * 求给定矩阵的广义逆矩阵
         * @tparam MatrixType
         * @param mat       输入矩阵
         * @param tolerance 误差
         * @return
         */
        template <typename MatrixType>
        MatrixType pseudoInverse(const MatrixType &mat, double tolerance = 1.e-8) {
            // TODO 有待测试，验证正确性

            Eigen::JacobiSVD<MatrixType> svd(mat,
                    Eigen::ComputeThinU | Eigen::ComputeThinV);

            MatrixType singularValuesInv = svd.singularValues();

            int cols = mat.cols();
            for (long i = 0; i < cols; ++i) {
                if (singularValuesInv(i) > tolerance) {
                    singularValuesInv(i) = 1.0 / singularValuesInv(i);
                } else {
                    singularValuesInv(i) = 0;
                }
            }

            return svd.matrixV()*singularValuesInv.asDiagonal()*svd.matrixU().transpose();
        } // pseudoInverse

    } // namespace Mat

} // namespace Mg

#endif //IMGPROCESS_MG_MATCOMMON_H
