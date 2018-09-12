//
// Created by penglei on 18-9-12.
//

#ifndef IMGPROCESS_IMGTOOL_ERROR_H
#define IMGPROCESS_IMGTOOL_ERROR_H

#include <string>

namespace ImgTool {

#define ERR_DRIVER_MSG "文件格式错误"
#define ERR_OPEN_DATASET_MSG "文件打开失败"
#define ERR_CREATE_DATASET_MSG "创建输出文件失败"
#define ERR_UNKNOWN_TYPE_MSG "不支持的数据类型"
#define ERR_MEMORY_MSG "分配内存失败"
#define ERR_COMPUTE_COVRIANCE_MSG "计算协方差矩阵失败"
#define ERR_MAT_NOT_INVERSE_MSG "矩阵不可逆"

    struct ErrorBase {
        void setErrorMsg(const std::string &msg) {
            errorMsg_ = msg;
        }

        const std::string& getErrorMsg() const { return errorMsg_; }

    protected:
        std::string errorMsg_;
    };


}

#endif //IMGPROCESS_IMGTOOL_ERROR_H
